#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>

#include <concepts>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>

namespace cp_api::math
{
    // --- Typedefs --- //
    using Vec2 = glm::vec2;
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;

    using IVec2 = glm::ivec2;
    using IVec3 = glm::ivec3;
    using IVec4 = glm::ivec4;

    using Mat3 = glm::mat3;
    using Mat4 = glm::mat4;

    using Quat = glm::quat;

    using uint = unsigned int;

    // --- Constantes matemáticas --- //
    constexpr float PI = glm::pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float DEG2RAD = PI / 180.0f;
    constexpr float RAD2DEG = 180.0f / PI;

    // --- Conversões --- //
    constexpr float ToRadians(float degrees) { return degrees * DEG2RAD; }
    constexpr float ToDegrees(float radians) { return radians * RAD2DEG; }

    // --- Clamp, Lerp, Remap --- //
    template <std::floating_point T>
    constexpr T Clamp(T v, T min, T max) { return std::clamp(v, min, max); }

    template <std::floating_point T>
    constexpr T Lerp(T a, T b, T t) { return glm::lerp(a, b, t); }

    template <std::floating_point T>
    constexpr T Remap(T value, T fromMin, T fromMax, T toMin, T toMax)
    {
        return toMin + ((value - fromMin) / (fromMax - fromMin)) * (toMax - toMin);
    }

    // --- Vetores --- //
    inline Vec3 Normalize(const Vec3& v) { return glm::normalize(v); }
    inline float Length(const Vec3& v) { return glm::length(v); }
    inline float Dot(const Vec3& a, const Vec3& b) { return glm::dot(a, b); }
    inline Vec3 Cross(const Vec3& a, const Vec3& b) { return glm::cross(a, b); }

    inline Vec3 Reflect(const Vec3& v, const Vec3& n) { return glm::reflect(v, n); }
    inline Vec3 Refract(const Vec3& v, const Vec3& n, float eta) { return glm::refract(v, n, eta); }

    // --- Matrizes --- //
    inline Mat4 Identity() { return glm::identity<Mat4>(); }
    inline Mat4 Translate(const Vec3& v) { return glm::translate(Mat4(1.0f), v); }
    inline Mat4 Scale(const Vec3& v) { return glm::scale(Mat4(1.0f), v); }
    inline Mat4 Rotate(float angleRad, const Vec3& axis) { return glm::rotate(Mat4(1.0f), angleRad, axis); }

    inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) { return glm::lookAt(eye, center, up); }
    inline Mat4 Perspective(float fovDeg, float aspect, float nearZ, float farZ)
    {
        return glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
    }

    inline Mat4 Ortho(float left, float right, float bottom, float top, float nearZ, float farZ)
    {
        return glm::ortho(left, right, bottom, top, nearZ, farZ);
    }

    // --- Quaternions --- //
    inline Quat FromEuler(const Vec3& eulerRad) { return glm::quat(eulerRad); }
    inline Vec3 ToEuler(const Quat& q) { return glm::eulerAngles(q); }
    inline Quat Normalize(const Quat& q) { return glm::normalize(q); }

    inline Quat Slerp(const Quat& a, const Quat& b, float t) { return glm::slerp(a, b, t); }

    // --- Utilitários gerais --- //
    template <typename T>
    inline std::string ToString(const T& value)
    {
        if constexpr (std::is_same_v<T, Vec2> || std::is_same_v<T, Vec3> || std::is_same_v<T, Vec4> ||
                      std::is_same_v<T, Mat4> || std::is_same_v<T, Quat>)
        {
            return glm::to_string(value);
        }
        else
        {
            std::ostringstream ss;
            ss << value;
            return ss.str();
        }
    }
} // namespace cp_api::math
