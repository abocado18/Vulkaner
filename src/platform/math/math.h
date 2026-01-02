#pragma once
#include <cmath>
#include <numbers>
#include <sys/types.h>

template <typename T> struct Vec3 {

  Vec3() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)) {};

  Vec3(T x, T y, T z) : x(x), y(y), z(z) {};

  T x, y, z;

  T operator*(const Vec3 &other) const {
    return (x * other.x + y * other.y + z * other.z);
  }

  Vec3 operator*(const T other) const {
    return {x * other, y * other, z * other};
  }

  Vec3 operator+(const Vec3 &other) const {
    return {x + other.x, y + other.y, z + other.z};
  }

  Vec3 operator-(const Vec3 &other) const {
    return {x - other.x, y - other.y, z - other.z};
  }

  Vec3 cross(const Vec3 &other) const {

    Vec3<T> cross = {};
    cross.x = y * other.z - z * other.y;
    cross.y = z * other.x - x * other.z;
    cross.z = x * other.y - y * other.x;

    return cross;
  }

  T length() const { return std::sqrt(x * x + y * y + z * z); }

  Vec3 normalized() const {
    T len = length();
    return len != 0 ? (*this) * (1 / len) : *this;
  }

  float dot(const Vec3<T> &other) const {

    return x * other.x + y * other.y + z * other.z;
  }

  constexpr static Vec3 forward() { return {0, 0, -1}; }
  constexpr static Vec3 right() { return {1, 0, 0}; }
  constexpr static Vec3 up() { return {0, 1, 0}; }
};

template <typename T> struct Vec2 {
  T x, y;

  T operator*(const Vec2 &other) const { return (x * other.x + y * other.y); }

  Vec2 operator*(const T other) const { return {x * other, y * other}; }

  Vec2 operator+(const Vec2 &other) const { return {x + other.x, y + other.y}; }

  Vec2 operator-(const Vec2 &other) const { return {x - other.x, y - other.y}; }

  T length() const { return std::sqrt(x * x + y * y); }

  Vec2 normalized() const {
    T len = length();
    return len != 0 ? (*this) * (1 / len) : *this;
  }
};

template <typename T> struct Vec4 {

  Vec4()
      : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)),
        w(static_cast<T>(0)) {};

  Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {};

  T x, y, z, w;

  T operator*(const Vec4 &other) const {
    return (x * other.x + y * other.y + z * other.z + w * other.w);
  }

  Vec4 operator*(const T other) const {
    return {x * other, y * other, z * other, w * other};
  }

  Vec4 operator+(const Vec4 &other) const {
    return {x + other.x, y + other.y, z + other.z, w + other.w};
  }

  Vec4 operator-(const Vec4 &other) const {
    return {x - other.x, y - other.y, z - other.z, w - other.w};
  }

  T length() const { return std::sqrt(x * x + y * y + z * z + w * w); }

  Vec4 normalized() const {
    T len = length();
    return len != 0 ? (*this) * (1 / len) : *this;
  }

  float dot(const Vec4<T> &other) const {

    return x * other.x + y * other.y + z * other.z + w * other.w;
  }
};

template <typename T> struct Quat {

  T x, y, z, w;

  static Quat identity() { return {0, 0, 0, 1}; }

  static Quat fromEuler(const Vec3<T> &euler) {

    T roll = euler.x;
    T pitch = euler.y;
    T yaw = euler.z;

    T cr = std::cos(roll * 0.5);
    T sr = std::sin(roll * 0.5);
    T cp = std::cos(pitch * 0.5);
    T sp = std::sin(pitch * 0.5);
    T cy = std::cos(yaw * 0.5);
    T sy = std::sin(yaw * 0.5);

    Quat<T> q;

    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return q;
  }

  Vec3<T> toEuler() const {

    Quat<T> q = this->normalized();

    Vec3<T> euler{};

    T sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    T cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    euler.x = std::atan2(sinr_cosp, cosr_cosp);

    T sinp = 2 * (q.w * q.y - q.x * q.z);
    if (std::abs(sinp) >= 1)
      euler.y = std::copysign(M_PI / 2, sinp);
    else
      euler.y = std::asin(sinp);

    T siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    T cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    euler.z = std::atan2(siny_cosp, cosy_cosp);

    return euler;
  }

  Quat<T> normalized() const {

    Quat<T> q = *this;

    T len = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    q.w /= len;
    q.x /= len;
    q.y /= len;
    q.z /= len;

    return q;
  }

  Vec3<T> operator*(const Vec3<T> &vec) const {

    Quat<T> quat_vec = {vec.x, vec.y, vec.z, 0};

    auto v = this->normalized();

    Quat<T> q_conj = v.conjugate();

    Quat<T> res = v * quat_vec * q_conj;

    return Vec3<T>{res.x, res.y, res.z};
  }

  Quat<T> conjugate() const { return Quat<T>{-x, -y, -z, w}; }

  Quat<T> operator*(const Quat<T> &rhs) const {
    return Quat<T>{w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                   w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                   w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
                   w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z};
  }
};

template <typename T> struct Mat4 {

  T values[16];

  static Mat4 identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  }

  static Mat4 transform(const Vec3<T> &transform) {
    Mat4 t = Mat4<T>::identity();
    t(0, 3) = transform.x;
    t(1, 3) = transform.y;
    t(2, 3) = transform.z;

    return t;
  }

  Mat4 applyRotation(const Quat<T> &rotation) const {
    Mat4 rotation_mat = rotationFromQuat(rotation);

    return *this * rotation_mat;
  }

  Mat4 applyRotation(const Mat4<T> &rotation_mat) const {
    return *this * rotation_mat;
  }

  Mat4 applyScale(const Vec3<T> scale) const {
    Mat4<T> scale_matrix = Mat4<T>::identity();
    scale_matrix(0, 0) = scale.x;
    scale_matrix(1, 1) = scale.y;
    scale_matrix(2, 2) = scale.z;

    return *this * scale_matrix;
  }

  static Mat4 createTransformMatrix(const Vec3<T> transform,
                                    const Vec3<T> scale,
                                    const Quat<T> rotation) {

    Mat4<T> t = Mat4<T>::transform(transform);

    Mat4<T> r = Mat4<T>::rotationFromQuat(rotation);

    Mat4<T> s = Mat4<T>::identity();
    s = s.applyScale(scale);

    return t * r * s;
  }

  bool isAffine() const {
    return values[3] == 0 && values[7] == 0 && values[11] == 0 &&
           values[15] == 1;
  }

  Mat4<T> transpose() {

    Mat4<T> transposed_m = {values[0], values[4], values[8],  values[12],
                            values[1], values[5], values[9],  values[13],
                            values[2], values[6], values[10], values[14],
                            values[3], values[7], values[11], values[15]};

    return transposed_m;
  };

  Mat4 inverse() {

    Mat4<T> inv = {};

    if (isAffine()) {

      for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
          inv(i, j) = (*this)(j, i);

      for (size_t i = 0; i < 3; ++i) {
        inv(i, 3) = 0;
        for (size_t j = 0; j < 3; ++j)
          inv(i, 3) -= inv(i, j) * (*this)(j, 3);
      }

      inv(3, 0) = inv(3, 1) = inv(3, 2) = 0;
      inv(3, 3) = 1;

    } else {

      const T *a = values;
      T *invOut = inv.values;

      invOut[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] -
                  a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
                  a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
      invOut[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] +
                  a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
                  a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
      invOut[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] -
                  a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
                  a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
      invOut[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] +
                  a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
                  a[9] * a[2] * a[7] + a[9] * a[3] * a[6];

      invOut[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] +
                  a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
                  a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
      invOut[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] -
                  a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
                  a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
      invOut[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] +
                  a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
                  a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
      invOut[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] -
                  a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
                  a[8] * a[2] * a[7] - a[8] * a[3] * a[6];

      invOut[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] -
                  a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
                  a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
      invOut[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] +
                  a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
                  a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
      invOut[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] -
                   a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
                   a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
      invOut[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] +
                   a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
                   a[8] * a[1] * a[7] + a[8] * a[3] * a[5];

      invOut[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] +
                   a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
                   a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
      invOut[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] -
                   a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
                   a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
      invOut[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] +
                   a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
                   a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
      invOut[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] -
                   a[4] * a[1] * a[10] + a[4] * a[2] * a[9] +
                   a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

      T det = a[0] * invOut[0] + a[1] * invOut[4] + a[2] * invOut[8] +
              a[3] * invOut[12];
      if (det == 0)
        return Mat4<T>();

      det = 1.0 / det;
      for (int i = 0; i < 16; ++i)
        invOut[i] *= det;
    }

    return inv;
  }

  static Mat4<T> rotationFromQuat(const Quat<T> &quat) {

    Quat<T> q = quat.normalized();

    Mat4<T> R = {};
    R(0, 0) = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
    R(0, 1) = 2 * q.x * q.y - 2 * q.w * q.z;
    R(0, 2) = 2 * q.x * q.z + 2 * q.w * q.y;
    R(0, 3) = 0;

    R(1, 0) = 2 * q.x * q.y + 2 * q.w * q.z;
    R(1, 1) = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
    R(1, 2) = 2 * q.y * q.z - 2 * q.w * q.x;
    R(1, 3) = 0;

    R(2, 0) = 2 * q.x * q.z - 2 * q.w * q.y;
    R(2, 1) = 2 * q.y * q.z + 2 * q.w * q.x;
    R(2, 2) = 1 - 2 * q.x * q.x - 2 * q.y * q.y;
    R(2, 3) = 0;

    R(3, 0) = 0;
    R(3, 1) = 0;
    R(3, 2) = 0;
    R(3, 3) = 1;

    return R;
  }

  static Mat4<T> perspective(T fov_y, T aspect, T near_plane, T far_plane) {

    T e = static_cast<T>(1) / (std::tan(fov_y / static_cast<T>(2)));

    Mat4<T> proj = Mat4<T>::identity();

    proj(0, 0) = e / aspect;
    proj(1, 1) = -e;

    proj(2, 2) = far_plane / (far_plane - near_plane);
    proj(2, 3) = -(far_plane * near_plane) / (far_plane - near_plane);

    proj(3, 2) = 1;
    proj(3, 3) = 0;

    return proj;
  }

  static Mat4<T> orthographic(T left, T right, T bottom, T top, T near_plane,
                              T far_plane) {
    Mat4<T> ortho = Mat4<T>::identity();

    ortho(0, 0) = 2 / (right - left);
    ortho(1, 1) = 2 / (top - bottom);
    ortho(2, 2) = -2 / (far_plane - near_plane);

    ortho(0, 3) = -((right + left) / (right - left));
    ortho(1, 3) = -((top + bottom) / (top - bottom));
    ortho(2, 3) = -((far_plane + near_plane) / (far_plane - near_plane));
    ortho(3, 3) = 1;

    return ortho;
  }

  static Mat4<T> lookAt(const Vec3<T> &eye, const Vec3<T> &up,
                        const Vec3<T> &center) {

    Vec3<T> forward_axis = (center - eye).normalized();

    Vec3<T> right_axis = forward_axis.cross(up).normalized();

    Vec3<T> up_axis = right_axis.cross(forward_axis).normalized();

    Vec3<T> translation;
    translation.x = eye.dot(right_axis);
    translation.y = eye.dot(up_axis);
    translation.z = eye.dot(forward_axis);

    Mat4<T> view = Mat4<T>::identity();

    view(0, 0) = right_axis.x;
    view(0, 1) = right_axis.y;
    view(0, 2) = right_axis.z;
    view(0, 3) = -translation.x;
    view(1, 0) = up_axis.x;
    view(1, 1) = up_axis.y;
    view(1, 2) = up_axis.z;
    view(1, 3) = -translation.y;
    view(2, 0) = forward_axis.x;
    view(2, 1) = forward_axis.y;
    view(2, 2) = forward_axis.z;
    view(2, 3) = -translation.z;
    view(3, 0) = 0;
    view(3, 1) = 0;
    view(3, 2) = 0;
    view(3, 3) = 1;

    return view;
  }

  Mat4<T> operator*(const Mat4<T> &other) const {

    Mat4<T> m;

    for (size_t i = 0; i < 4; i++) {

      for (size_t j = 0; j < 4; j++) {
        m(i, j) = 0;

        for (size_t k = 0; k < 4; k++) {
          m(i, j) += (*this)(i, k) * other(k, j);
        }
      }
    }

    return m;
  }

  Mat4<T> operator+(const Mat4<T> &other) const {

    Mat4<T> p;

    for (size_t i = 0; i < 4; i++) {
      for (size_t j = 0; j < 4; j++) {
        p(i, j) = (*this)(i, j) + other(i, j);
      }
    }

    return p;
  }

  Mat4<T> operator-(const Mat4<T> &other) const {

    Mat4<T> m;

    for (size_t i = 0; i < 4; i++) {
      for (size_t j = 0; j < 4; j++) {
        m(i, j) = (*this)(i, j) - other(i, j);
      }
    }

    return m;
  }

  Vec4<T> const operator*(const Vec4<T> v) const {

    Mat4<T> &m = *this;

    Vec4<T> return_vec{};
    return_vec.x =
        v.x * m(0, 0) + v.y * m(0, 1) + v.z * m(0, 2) + v.w * m(0, 3);
    return_vec.y =
        v.x * m(1, 0) + v.y * m(1, 1) + v.z * m(1, 2) + v.w * m(1, 3);
    return_vec.z =
        v.x * m(2, 0) + v.y * m(2, 1) + v.z * m(2, 2) + v.w * m(2, 3);
    return_vec.w =
        v.x * m(3, 0) + v.y * m(3, 1) + v.z * m(3, 2) + v.w * m(3, 3);
  }

  T &operator()(const size_t i, const size_t j) { return values[i * 4 + j]; }

  const T &operator()(const size_t i, const size_t j) const {
    return values[i * 4 + j];
  }
};
