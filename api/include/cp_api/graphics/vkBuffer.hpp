#pragma once

#include <utility>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

#include "glfw.inc.hpp"
#include "vma.inc.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {

    // ---------- small helper struct for async upload handles ----------
    struct UploadHandle {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;

        bool valid() const noexcept { return fence != VK_NULL_HANDLE || cmd != VK_NULL_HANDLE; }
    };

    // ---------- StagingRing: a reusable mapped staging buffer used as ring allocator ----------
    class StagingRing {
    public:
        // create a staging ring of totalSize bytes (mappable). It will be created mapped.
        StagingRing() = default;
        StagingRing(VmaAllocator allocator, VkDeviceSize totalSize, VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            : allocator(allocator), totalSize(totalSize), usage(usage), running(false)
        {
            if (allocator == VK_NULL_HANDLE) CP_LOG_THROW("StagingRing: allocator null");
            create();
            startJanitor();
        }

        ~StagingRing() {
            stopJanitor();
            destroy();
        }

        StagingRing(StagingRing&& o) noexcept { *this = std::move(o); }
        StagingRing& operator=(StagingRing&& o) noexcept {
            if (this == &o) return *this;
            stopJanitor();
            destroy();
            allocator = o.allocator;
            buffer = o.buffer;
            allocation = o.allocation;
            allocationInfo = o.allocationInfo;
            mapped = o.mapped;
            totalSize = o.totalSize;
            usage = o.usage;
            head = o.head;
            tail = o.tail;
            // transfer janitor queue/state (not running)
            o.buffer = VK_NULL_HANDLE;
            o.allocation = VK_NULL_HANDLE;
            o.allocationInfo = {};
            o.mapped = nullptr;
            o.totalSize = 0;
            o.head = o.tail = 0;
            return *this;
        }

        StagingRing(const StagingRing&) = delete;
        StagingRing& operator=(const StagingRing&) = delete;

        // Reserve a region of `size` bytes (aligned to align bytes). Returns offset in staging buffer + pointer.
        // If size > totalSize => throws.
        // This is a single-writer ring by design; protect externally for multithreaded producers or use a mutex.
        struct Reservation {
            VkDeviceSize offset;
            void* ptr;
        };

        Reservation Reserve(VkDeviceSize size, VkDeviceSize align = 16) {
            std::lock_guard<std::mutex> lk(commitMutex); // protect head updates
            if (size > totalSize) CP_LOG_THROW("StagingRing::Reserve size > totalSize");
            // align head
            VkDeviceSize alignedHead = Align(head, align);

            if (alignedHead + size <= totalSize) {
                // fits until end
                Reservation r{ alignedHead, static_cast<uint8_t*>(mapped) + alignedHead };
                head = alignedHead + size;
                return r;
            } else {
                // wrap to start
                alignedHead = Align(0, align);
                if (alignedHead + size > tail) {
                    // no space (tail is occupied region not yet freed) -> we cannot alloc
                    // Caller must ensure previous uploads finished or use larger ring.
                    CP_LOG_THROW("StagingRing::Reserve out of space - increase ring size or ensure GPU consumed previous uploads");
                }
                Reservation r{ alignedHead, static_cast<uint8_t*>(mapped) + alignedHead };
                head = alignedHead + size;
                return r;
            }
        }

        // Mark region starting at `offset` length `size` as free by advancing tail (call when GPU consumed data)
        // Prefer to use SubmitAndTrack(...) so tail advances automatically when the associated fence completes.
        void AdvanceTailTo(VkDeviceSize newTail) {
            std::lock_guard<std::mutex> lk(commitMutex);
            tail = newTail % totalSize;
        }

        VkBuffer GetBuffer() const noexcept { return buffer; }
        VkDeviceSize GetTotalSize() const noexcept { return totalSize; }
        void* GetMappedPtr() const noexcept { return mapped; }

        // Utility to compute aligned positions
        static VkDeviceSize Align(VkDeviceSize v, VkDeviceSize align) {
            return (v + (align - 1)) & ~(align - 1);
        }

        VmaAllocation getAllocation() const noexcept { return allocation; }
        VmaAllocator getAllocator() const noexcept { return allocator; }

        // Register a pending upload tied to a fence; when fence completes, janitor will advance tail automatically.
        // offset: start offset in ring; size: length; handle: upload handle created by the uploader.
        void SubmitAndTrack(UploadHandle handle, VkDeviceSize offset, VkDeviceSize size) {
            if (!handle.valid()) return;
            Pending p;
            p.handle = handle;
            p.offsetEnd = (offset + size) % totalSize;
            {
                std::lock_guard<std::mutex> lk(queueMutex);
                pending.push(std::move(p));
            }
            cv.notify_one();
        }

    private:
        void create() {
            VkBufferCreateInfo bci{};
            bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bci.size = totalSize;
            bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo aci{};
            aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // already mapped

            if (vmaCreateBuffer(allocator, &bci, &aci, &buffer, &allocation, &allocationInfo) != VK_SUCCESS) {
                CP_LOG_THROW("StagingRing::create failed");
            }

            // vma provides pMappedData in allocationInfo when mapped
            mapped = allocationInfo.pMappedData;
            head = tail = 0;
        }

        void destroy() {
            if (buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator, buffer, allocation);
            }
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            allocationInfo = {};
            mapped = nullptr;
            totalSize = 0;
            head = tail = 0;
        }

        struct Pending {
            UploadHandle handle;
            VkDeviceSize offsetEnd;
        };

        void startJanitor() {
            running.store(true);
            janitor = std::thread([this](){ this->janitorLoop(); });
        }

        void stopJanitor() {
            running.store(false);
            cv.notify_one();
            if (janitor.joinable()) janitor.join();
            // cleanup any remaining pending handles
            std::lock_guard<std::mutex> lk(queueMutex);
            while(!pending.empty()) {
                auto p = pending.front(); pending.pop();
                if (p.handle.valid()) {
                    if (p.handle.device && p.handle.fence) {
                        vkWaitForFences(p.handle.device, 1, &p.handle.fence, VK_TRUE, UINT64_MAX);
                        vkDestroyFence(p.handle.device, p.handle.fence, nullptr);
                    }
                    if (p.handle.device && p.handle.cmd && p.handle.pool) {
                        vkFreeCommandBuffers(p.handle.device, p.handle.pool, 1, &p.handle.cmd);
                    }
                }
            }
        }

        void janitorLoop() {
            while (running.load()) {
                Pending item;
                {
                    std::unique_lock<std::mutex> lk(queueMutex);
                    cv.wait(lk, [this]{ return !pending.empty() || !running.load(); });
                    if (!running.load() && pending.empty()) break;
                    item = pending.front(); pending.pop();
                }

                if (!item.handle.valid()) continue;
                // wait for fence (block here per item) then cleanup and advance tail
                if (item.handle.device && item.handle.fence) {
                    vkWaitForFences(item.handle.device, 1, &item.handle.fence, VK_TRUE, UINT64_MAX);
                    vkDestroyFence(item.handle.device, item.handle.fence, nullptr);
                }
                if (item.handle.device && item.handle.cmd && item.handle.pool) {
                    vkFreeCommandBuffers(item.handle.device, item.handle.pool, 1, &item.handle.cmd);
                }

                // advance tail to offsetEnd
                {
                    std::lock_guard<std::mutex> lk(commitMutex);
                    tail = item.offsetEnd;
                }
            }
        }

    private:
        VmaAllocator allocator = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};
        void* mapped = nullptr;
        VkDeviceSize totalSize = 0;
        VkBufferUsageFlags usage = 0;

        // ring state
        VkDeviceSize head = 0; // next free position to allocate from
        VkDeviceSize tail = 0; // oldest not-yet-consumed position

        // synchronization
        std::mutex commitMutex; // protects head/tail

        // janitor queue
        std::mutex queueMutex;
        std::condition_variable cv;
        std::queue<Pending> pending;
        std::thread janitor;
        std::atomic<bool> running{false};
    };

    // ---------- VulkanBuffer PRO (with staging ring usage, async cleanup and resize-preserve) ----------
    class VulkanBuffer {
    public:
        VulkanBuffer() = default;

        VulkanBuffer(VmaAllocator allocator,
                    VkDeviceSize size,
                    VkBufferUsageFlags usage,
                    VmaMemoryUsage memUsage,
                    bool persistentlyMapped = false)
            : allocator(allocator),
            usage(usage),
            size(size),
            persistentlyMapped(persistentlyMapped)
        {
            createInternal(size, usage, memUsage);
            if (persistentlyMapped) {
                if (allocationInfo.pMappedData) mappedPtr = allocationInfo.pMappedData;
                else {
                    // try explicit map
                    void* p = nullptr;
                    if (vmaMapMemory(allocator, allocation, &p) == VK_SUCCESS) mappedPtr = p;
                    else {
                        destroy();
                        CP_LOG_THROW("Requested persistentlyMapped but allocation isn't mappable");
                    }
                }
            }
        }

        ~VulkanBuffer() {
            destroy();
        }

        VulkanBuffer(VulkanBuffer&& o) noexcept { *this = std::move(o); }
        VulkanBuffer& operator=(VulkanBuffer&& o) noexcept {
            if (this == &o) return *this;
            destroy();

            buffer = o.buffer;
            allocator = o.allocator;
            allocation = o.allocation;
            allocationInfo = o.allocationInfo;
            usage = o.usage;
            size = o.size;
            persistentlyMapped = o.persistentlyMapped;
            mappedPtr = o.mappedPtr;

            o.buffer = VK_NULL_HANDLE;
            o.allocator = VK_NULL_HANDLE;
            o.allocation = VK_NULL_HANDLE;
            o.allocationInfo = {};
            o.usage = 0;
            o.size = 0;
            o.persistentlyMapped = false;
            o.mappedPtr = nullptr;
            return *this;
        }

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;

        // Accessors
        VkBuffer GetBuffer() const noexcept { return buffer; }
        VmaAllocation GetAllocation() const noexcept { return allocation; }
        const VmaAllocationInfo& GetAllocationInfo() const noexcept { return allocationInfo; }
        VkBufferUsageFlags GetUsage() const noexcept { return usage; }
        VkDeviceSize GetSize() const noexcept { return size; }
        bool IsPersistentlyMapped() const noexcept { return persistentlyMapped; }

        // Map/unmap helpers
        void Map() {
            if (mappedPtr) return;
            if (allocation == VK_NULL_HANDLE) throw std::runtime_error("VulkanBuffer::Map: no allocation");
            if (vmaMapMemory(allocator, allocation, &mappedPtr) != VK_SUCCESS) {
                CP_LOG_THROW("VulkanBuffer::Map failed");
            }
        }
        void Unmap() {
            if (!mappedPtr) return;
            vmaUnmapMemory(allocator, allocation);
            mappedPtr = nullptr;
        }

        // Write with flush
        void Write(const void* src, VkDeviceSize sz, VkDeviceSize offset = 0) {
            if (!src) throw std::invalid_argument("VulkanBuffer::Write: src is null");
            if (offset + sz > size) throw std::out_of_range("VulkanBuffer::Write: write exceeds buffer size");
            if (allocation == VK_NULL_HANDLE) throw std::runtime_error("VulkanBuffer::Write: buffer not allocated");

            if (mappedPtr) {
                std::memcpy(static_cast<uint8_t*>(mappedPtr) + offset, src, static_cast<size_t>(sz));
                vmaFlushAllocation(allocator, allocation, offset, sz);
            } else {
                void* p = nullptr;
                if (vmaMapMemory(allocator, allocation, &p) != VK_SUCCESS) CP_LOG_THROW("VulkanBuffer::Write map failed");
                std::memcpy(static_cast<uint8_t*>(p) + offset, src, static_cast<size_t>(sz));
                vmaFlushAllocation(allocator, allocation, offset, sz);
                vmaUnmapMemory(allocator, allocation);
            }
        }

        // Classic upload that creates a temporary staging buffer (blocking/non-blocking)
        UploadHandle Upload(VkDevice device,
                            VkCommandPool cmdPool,
                            VkQueue queue,
                            const void* srcData,
                            VkDeviceSize uploadSize,
                            bool wait = true)
        {
            if (!srcData) throw std::invalid_argument("VulkanBuffer::Upload: srcData null");
            if (uploadSize == 0 || uploadSize > size) throw std::invalid_argument("VulkanBuffer::Upload: bad size");
            if (buffer == VK_NULL_HANDLE) throw std::runtime_error("VulkanBuffer::Upload: destination buffer not created");

            // Create staging temp (mapped)
            VulkanBuffer staging(allocator, uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, true);
            staging.Write(srcData, uploadSize, 0);

            // Allocate one-time command buffer
            VkCommandBufferAllocateInfo cbAlloc{};
            cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAlloc.commandPool = cmdPool;
            cbAlloc.commandBufferCount = 1;

            VkCommandBuffer cmd{};
            if (vkAllocateCommandBuffers(device, &cbAlloc, &cmd) != VK_SUCCESS) {
                CP_LOG_THROW("VulkanBuffer::Upload failed to allocate command buffer");
            }

            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &begin);

            VkBufferCopy region{};
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = uploadSize;
            vkCmdCopyBuffer(cmd, staging.GetBuffer(), this->buffer, 1, &region);

            vkEndCommandBuffer(cmd);

            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;
            if (vkCreateFence(device, &fci, nullptr, &fence) != VK_SUCCESS) {
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("VulkanBuffer::Upload failed to create fence");
            }

            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd;

            if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) {
                vkDestroyFence(device, fence, nullptr);
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("VulkanBuffer::Upload failed to submit");
            }

            UploadHandle handle;
            handle.fence = fence;
            handle.cmd = cmd;
            handle.device = device;
            handle.pool = cmdPool;

            if (wait) {
                vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
                vkDestroyFence(device, fence, nullptr);
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                return UploadHandle{};
            } else {
                return handle;
            }
        }

        // Upload using a provided StagingRing (preferível para muitos uploads)
        // reserveSize: size to reserve in ring; align: alignment (defaults 16)
        // returns an UploadHandle (wait==false) or invalid handle if wait==true (blocking)
        UploadHandle UploadUsingRing(StagingRing& ring,
                                    VkDevice device,
                                    VkCommandPool cmdPool,
                                    VkQueue queue,
                                    const void* srcData,
                                    VkDeviceSize uploadSize,
                                    VkDeviceSize align = 16,
                                    bool wait = true)
        {
            if (!srcData) throw std::invalid_argument("VulkanBuffer::UploadUsingRing: srcData null");
            if (uploadSize == 0 || uploadSize > size) throw std::invalid_argument("VulkanBuffer::UploadUsingRing: bad size");
            if (ring.GetBuffer() == VK_NULL_HANDLE) CP_LOG_THROW("Staging ring not created");

            // Reserve space in ring
            auto res = ring.Reserve(uploadSize, align);
            // copy into mapped staging ring
            std::memcpy(res.ptr, srcData, static_cast<size_t>(uploadSize));
            // flush allocation region (the ring uses VMA CPU_TO_GPU mapped)
            vmaFlushAllocation(ringAllocator(ring), ringAllocation(ring), res.offset, uploadSize); // helper functions below

            // create command buffer
            VkCommandBufferAllocateInfo cbAlloc{};
            cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAlloc.commandPool = cmdPool;
            cbAlloc.commandBufferCount = 1;

            VkCommandBuffer cmd{};
            if (vkAllocateCommandBuffers(device, &cbAlloc, &cmd) != VK_SUCCESS) {
                CP_LOG_THROW("VulkanBuffer::UploadUsingRing failed to allocate command buffer");
            }

            VkCommandBufferBeginInfo bbi{};
            bbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            bbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &bbi);

            VkBufferCopy region{};
            region.srcOffset = res.offset;
            region.dstOffset = 0;
            region.size = uploadSize;
            vkCmdCopyBuffer(cmd, ring.GetBuffer(), this->buffer, 1, &region);

            vkEndCommandBuffer(cmd);

            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;
            if (vkCreateFence(device, &fci, nullptr, &fence) != VK_SUCCESS) {
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("VulkanBuffer::UploadUsingRing failed to create fence");
            }

            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd;

            if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) {
                vkDestroyFence(device, fence, nullptr);
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("VulkanBuffer::UploadUsingRing failed to submit");
            }

            // Advance tail is left to user — but we can optionally compute minimal consumption later.
            // For simplicity, we do NOT change ring.tail here: user may call ring.AdvanceTailTo(...) with appropriate value
            // after waiting for fence. Alternatively use UploadAndFreeWhenComplete(handle) to advance tail automatically.

            UploadHandle handle;
            handle.fence = fence;
            handle.cmd = cmd;
            handle.device = device;
            handle.pool = cmdPool;

            if (wait) {
                vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
                vkDestroyFence(device, fence, nullptr);
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                // Optionally advance ring.tail fully here (not done because we don't know which offset consumed previously)
                return UploadHandle{};
            } else {
                // Track this upload with the ring so the ring janitor will advance tail when the fence completes
                ring.SubmitAndTrack(handle, res.offset, uploadSize);
                return handle;
            }
        }

        // Convenience: call to wait for fence and free cmd + destroy fence in background thread.
        // This helper detaches a thread that blocks on vkWaitForFences and cleans resources.
        static void UploadAndFreeWhenComplete(UploadHandle handle) {
            if (!handle.valid()) return;
            // detach a thread that waits and then cleans resources
            std::thread([h = handle]() mutable {
                if (h.device == VK_NULL_HANDLE) return;
                vkWaitForFences(h.device, 1, &h.fence, VK_TRUE, UINT64_MAX);
                vkDestroyFence(h.device, h.fence, nullptr);
                if (h.cmd != VK_NULL_HANDLE && h.pool != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(h.device, h.pool, 1, &h.cmd);
                }
            }).detach();
        }

        // ResizePreserve: create a new buffer with newSize and copy old contents (up to min(old,new)) using GPU copy.
        // Note: new buffer uses memUsageNew for allocation.
        // Requires device/cmdPool/queue to perform copy.
        void ResizePreserve(VkDeviceSize newSize, VmaMemoryUsage memUsageNew, VkDevice device, VkCommandPool cmdPool, VkQueue queue) {
            if (newSize == size) return;
            if (buffer == VK_NULL_HANDLE) {
                // nothing to preserve, just create new
                Resize(newSize, memUsageNew);
                return;
            }

            // create new buffer object
            VulkanBuffer newBuf(allocator, newSize, usage, memUsageNew, false);

            // ensure both buffers have TRANSFER_SRC/TRANSFER_DST bits
            // allocate command buffer
            VkCommandBufferAllocateInfo cbAlloc{};
            cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAlloc.commandPool = cmdPool;
            cbAlloc.commandBufferCount = 1;

            VkCommandBuffer cmd{};
            if (vkAllocateCommandBuffers(device, &cbAlloc, &cmd) != VK_SUCCESS) {
                CP_LOG_THROW("ResizePreserve: failed to allocate command buffer");
            }

            VkCommandBufferBeginInfo bbi{};
            bbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            bbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &bbi);

            VkBufferCopy copy{};
            copy.srcOffset = 0;
            copy.dstOffset = 0;
            copy.size = (size < newSize) ? size : newSize;
            if (copy.size > 0)
                vkCmdCopyBuffer(cmd, this->buffer, newBuf.buffer, 1, &copy);

            vkEndCommandBuffer(cmd);

            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;
            if (vkCreateFence(device, &fci, nullptr, &fence) != VK_SUCCESS) {
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("ResizePreserve: failed to create fence");
            }

            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cmd;

            if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) {
                vkDestroyFence(device, fence, nullptr);
                vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
                CP_LOG_THROW("ResizePreserve: failed to submit");
            }

            // wait and cleanup
            vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(device, fence, nullptr);
            vkFreeCommandBuffers(device, cmdPool, 1, &cmd);

            // swap into this
            this->destroy();
            *this = std::move(newBuf);
        }

        // Classic Resize: destroys old buffer and creates new (no preservation)
        void Resize(VkDeviceSize newSize, VmaMemoryUsage memUsage) {
            if (newSize == size) return;
            destroy();
            createInternal(newSize, usage, memUsage);
            if (persistentlyMapped) {
                if (vmaMapMemory(allocator, allocation, &mappedPtr) != VK_SUCCESS) {
                    CP_LOG_THROW("VulkanBuffer::Resize: failed to map new allocation");
                }
            }
        }

        void destroy() {
            if (mappedPtr && allocation != VK_NULL_HANDLE && !persistentlyMapped) {
                vmaUnmapMemory(allocator, allocation);
                mappedPtr = nullptr;
            }

            if (buffer != VK_NULL_HANDLE) {
                CP_LOG_INFO("VulkanBuffer::destroy size {}", allocationInfo.size);
                vmaDestroyBuffer(allocator, buffer, allocation);
            }

            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            allocationInfo = {};
            usage = 0;
            size = 0;
            allocator = VK_NULL_HANDLE;
            persistentlyMapped = false;
            mappedPtr = nullptr;
        }

    private:
        void createInternal(VkDeviceSize createSize, VkBufferUsageFlags createUsage, VmaMemoryUsage memUsage) {
            if (allocator == VK_NULL_HANDLE) CP_LOG_THROW("VulkanBuffer: allocator is null");

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = createSize;
            bufferInfo.usage = createUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = memUsage;
            allocInfo.flags = 0;

            if (persistentlyMapped) {
                allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            }

            this->allocator = allocator;
            this->usage = createUsage;
            this->size = createSize;

            if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo) != VK_SUCCESS) {
                CP_LOG_THROW("VulkanBuffer::createInternal failed to create buffer via VMA");
            }

            CP_LOG_INFO("VulkanBuffer::created size {}, is persistantly mapped? {}", allocationInfo.size, persistentlyMapped);

            if (persistentlyMapped && allocationInfo.pMappedData) {
                mappedPtr = allocationInfo.pMappedData;
            }
        }

        // Helpers to access private staging ring internals (ugly but local)
        static VmaAllocator ringAllocator(StagingRing& r) {
            return r.getAllocator();
        }
        static VmaAllocation ringAllocation(StagingRing& r) {
            return r.getAllocation();
        }

    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo{};
        VkBufferUsageFlags usage = 0;
        VkDeviceSize size = 0;

        bool persistentlyMapped = false;
        void* mappedPtr = nullptr;
    };
} // namespace cp_api
