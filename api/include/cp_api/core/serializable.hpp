#pragma once

#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <optional>
#include <memory>
#include <type_traits>
#include <functional>
#include <nlohmann/json.hpp>

namespace cp_api {

    class SerializableBase {
    public:
        virtual ~SerializableBase() = default;

        // --------------------------
        // Registro de campos
        // --------------------------
        template<typename T>
        void RegisterField(const std::string& name, T& field) {
            m_fields[name] = [&field]() -> nlohmann::json {
                return SerializeField(field);
            };
            m_deserializers[name] = [&field](const nlohmann::json& j) {
                DeserializeField(field, j);
            };
        }

        // Serializa todos os campos para JSON
        nlohmann::json Serialize() const {
            nlohmann::json j;
            for (auto& [name, getter] : m_fields) {
                j[name] = getter();
            }
            return j;
        }

        // Serializa todos os campos para BSON
        std::vector<uint8_t> SerializeBSON() const {
            return nlohmann::json::to_bson(Serialize());
        }

        // Desserializa todos os campos de JSON
        void Deserialize(const nlohmann::json& j) {
            for (auto& [name, setter] : m_deserializers) {
                if (j.contains(name)) {
                    setter(j.at(name));
                }
            }
        }

        // Desserializa todos os campos de BSON
        void DeserializeBSON(const std::vector<uint8_t>& data) {
            Deserialize(nlohmann::json::from_bson(data));
        }

    protected:
        // --------------------------
        // Serialização genérica
        // --------------------------
        template<typename T>
        static nlohmann::json SerializeField(const T& value) {
            if constexpr (std::is_base_of<SerializableBase, T>::value) {
                return value.Serialize();
            } else if constexpr (is_vector<T>::value || is_array<T>::value) {
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& el : value) {
                    arr.push_back(SerializeField(el));
                }
                return arr;
            } else if constexpr (is_map<T>::value || is_unordered_map<T>::value) {
                nlohmann::json obj = nlohmann::json::object();
                for (const auto& [k, v] : value) {
                    obj[k] = SerializeField(v);
                }
                return obj;
            } else if constexpr (is_optional<T>::value) {
                if (value.has_value()) return SerializeField(*value);
                else return nullptr;
            } else if constexpr (is_unique_ptr<T>::value) {
                if (value) return SerializeField(*value);
                else return nullptr;
            } else {
                return value;
            }
        }

        template<typename T>
        static void DeserializeField(T& field, const nlohmann::json& j) {
            if constexpr (std::is_base_of<SerializableBase, T>::value) {
                field.Deserialize(j);
            } else if constexpr (is_vector<T>::value) {
                field.clear();
                for (const auto& el : j) {
                    typename T::value_type tmp;
                    DeserializeField(tmp, el);
                    field.push_back(std::move(tmp));
                }
            } else if constexpr (is_array<T>::value) {
                size_t idx = 0;
                for (const auto& el : j) {
                    DeserializeField(field[idx++], el);
                }
            } else if constexpr (is_map<T>::value || is_unordered_map<T>::value) {
                field.clear();
                for (auto it = j.begin(); it != j.end(); ++it) {
                    typename T::mapped_type tmp;
                    DeserializeField(tmp, it.value());
                    field[it.key()] = std::move(tmp);
                }
            } else if constexpr (is_optional<T>::value) {
                if (j.is_null()) field.reset();
                else {
                    typename T::value_type tmp;
                    DeserializeField(tmp, j);
                    field = std::move(tmp);
                }
            } else if constexpr (is_unique_ptr<T>::value) {
                if (j.is_null()) field.reset();
                else {
                    field = std::make_unique<typename T::element_type>();
                    DeserializeField(*field, j);
                }
            } else {
                field = j.get<T>();
            }
        }

    private:
        std::unordered_map<std::string, std::function<nlohmann::json()>> m_fields;
        std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> m_deserializers;

        // --------------------------
        // Traits auxiliares
        // --------------------------
        template<typename T> struct is_vector : std::false_type {};
        template<typename... Args> struct is_vector<std::vector<Args...>> : std::true_type {};

        template<typename T> struct is_array : std::false_type {};
        template<typename U, std::size_t N> struct is_array<std::array<U, N>> : std::true_type {};

        template<typename T> struct is_map : std::false_type {};
        template<typename... Args> struct is_map<std::map<Args...>> : std::true_type {};

        template<typename T> struct is_unordered_map : std::false_type {};
        template<typename... Args> struct is_unordered_map<std::unordered_map<Args...>> : std::true_type {};

        template<typename T> struct is_optional : std::false_type {};
        template<typename... Args> struct is_optional<std::optional<Args...>> : std::true_type {};

        template<typename T> struct is_unique_ptr : std::false_type {};
        template<typename U> struct is_unique_ptr<std::unique_ptr<U>> : std::true_type {};
    };

} // namespace cp_api
