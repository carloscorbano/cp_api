#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <vector>
#include <functional>
#include <type_traits>
#include <iostream>

/**
 * EXEMPLO DE USO DO ARQUIVO:
 * @code
 *   #include "serializable.hpp"
 *   #include <string>
 *   #include <vector>
 *
 *   struct Stats : public cp_api::SerializableBase {
 *       int strength = 10;
 *       int agility = 8;
 *
 *       Stats() {
 *           RegisterField(SERIALIZABLE_FIELD(strength));
 *           RegisterField(SERIALIZABLE_FIELD(agility));
 *       }
 *   };
 *
 *   struct Player : public cp_api::SerializableBase {
 *       std::string name = "Hero";
 *       int level = 1;
 *       Stats stats;
 *       std::vector<Stats> inventory; // array de objetos serializáveis
 *
 *       Player() {
 *           RegisterField(SERIALIZABLE_FIELD(name));
 *           RegisterField(SERIALIZABLE_FIELD(level));
 *           RegisterField(SERIALIZABLE_FIELD(stats));
 *           RegisterField(SERIALIZABLE_FIELD(inventory));
 *       }
 *   };
 */

namespace cp_api {

// ---------------------------
// Interface base para todos os objetos serializáveis
// ---------------------------
class ISerializable {
public:
    virtual ~ISerializable() = default;
    virtual nlohmann::json Serialize() const = 0;
    virtual void Deserialize(const nlohmann::json& j) = 0;
};

// ---------------------------
// Helpers JSON / BSON
// ---------------------------
inline bool SaveJsonToFile(const ISerializable& obj, const std::string& path, bool pretty = true) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        nlohmann::json j = obj.Serialize();
        file << (pretty ? j.dump(4) : j.dump());
        return true;
    } catch (const std::exception& e) { std::cerr << e.what(); return false; }
}

inline bool LoadJsonFromFile(ISerializable& obj, const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        nlohmann::json j;
        file >> j;
        obj.Deserialize(j);
        return true;
    } catch (const std::exception& e) { std::cerr << e.what(); return false; }
}

inline bool SaveBsonToFile(const ISerializable& obj, const std::string& path) {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        auto bsonData = nlohmann::json::to_bson(obj.Serialize());
        file.write(reinterpret_cast<const char*>(bsonData.data()), bsonData.size());
        return true;
    } catch (const std::exception& e) { std::cerr << e.what(); return false; }
}

inline bool LoadBsonFromFile(ISerializable& obj, const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        nlohmann::json j = nlohmann::json::from_bson(buffer);
        obj.Deserialize(j);
        return true;
    } catch (const std::exception& e) { std::cerr << e.what(); return false; }
}

// ---------------------------
// Classe base para objetos com campos registrados
// ---------------------------
class SerializableBase : public ISerializable {
public:
    using Getter = std::function<nlohmann::json()>;
    using Setter = std::function<void(const nlohmann::json&)>;
    struct Field { std::string name; Getter getter; Setter setter; };

    nlohmann::json Serialize() const override {
        nlohmann::json j;
        for (const auto& f : m_fields) j[f.name] = f.getter();
        return j;
    }

    void Deserialize(const nlohmann::json& j) override {
        for (const auto& f : m_fields) {
            if (j.contains(f.name)) f.setter(j[f.name]);
        }
    }

protected:
    void RegisterField(const Field& f) { m_fields.push_back(f); }

    template<typename T>
    static nlohmann::json SerializeField(const T& value) {
        if constexpr (std::is_base_of_v<ISerializable, T>) {
            return value.Serialize(); // aninhamento recursivo
        } else {
            return value;
        }
    }

    template<typename T>
    static void DeserializeField(T& value, const nlohmann::json& j) {
        if constexpr (std::is_base_of_v<ISerializable, T>) {
            value.Deserialize(j);
        } else {
            value = j.get<T>();
        }
    }

    std::vector<Field> m_fields;
};

// ---------------------------
// Macro para facilitar registro de campos
// ---------------------------
#define SERIALIZABLE_FIELD(name) \
    { #name, [&]() -> nlohmann::json { return SerializableBase::SerializeField(name); }, \
             [&](const nlohmann::json& j) { SerializableBase::DeserializeField(name, j); } }

} // namespace cp_api
