#include "cp_api/framework.hpp"
#include "cp_api/core/debug.hpp"

namespace cp_api {
    Framework::Framework() {
        CP_LOG_INFO("Framework constructed.");
    }

    Framework::~Framework() {
        CP_LOG_INFO("Framework destructed.");
    }

    void Framework::Init() {
        CP_LOG_INFO("Framework initialized.");
    }

    void Framework::Run() {
        CP_LOG_INFO("Framework running.");
    }
} // namespace cp_api