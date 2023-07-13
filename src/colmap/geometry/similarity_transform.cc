// Copyright (c) 2023, ETH Zurich and UNC Chapel Hill.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of ETH Zurich and UNC Chapel Hill nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: Johannes L. Schoenberger (jsch-at-demuc-dot-de)

#include "colmap/geometry/similarity_transform.h"

#include "colmap/estimators/similarity_transform.h"
#include "colmap/geometry/pose.h"
#include "colmap/geometry/projection.h"

#include <fstream>

namespace colmap {

SimilarityTransform3::SimilarityTransform3()
    : matrix_(Eigen::Matrix3x4d::Identity()) {}

SimilarityTransform3::SimilarityTransform3(const Eigen::Matrix3x4d& matrix)
    : matrix_(matrix) {}

SimilarityTransform3::SimilarityTransform3(const double scale,
                                           const Eigen::Vector4d& qvec,
                                           const Eigen::Vector3d& tvec) {
  matrix_ = ComposeProjectionMatrix(qvec, tvec);
  matrix_.leftCols<3>() *= scale;
}

SimilarityTransform3 SimilarityTransform3::Inverse() const {
  const double scale = Scale();
  Eigen::Matrix3x4d inverse;
  inverse.leftCols<3>() = matrix_.leftCols<3>().transpose() / (scale * scale);
  inverse.col(3) = inverse.leftCols<3>() * -matrix_.col(3);
  return SimilarityTransform3(inverse);
}

const Eigen::Matrix3x4d& SimilarityTransform3::Matrix() const {
  return matrix_;
}

double SimilarityTransform3::Scale() const { return matrix_.col(0).norm(); }

Eigen::Vector4d SimilarityTransform3::Rotation() const {
  return RotationMatrixToQuaternion(matrix_.leftCols<3>() / Scale());
}

Eigen::Vector3d SimilarityTransform3::Translation() const {
  return matrix_.col(3);
}

bool SimilarityTransform3::Estimate(const std::vector<Eigen::Vector3d>& src,
                                    const std::vector<Eigen::Vector3d>& tgt) {
  const auto results =
      SimilarityTransformEstimator<3, true>().Estimate(src, tgt);
  if (results.empty()) {
    return false;
  }
  CHECK_EQ(results.size(), 1);
  matrix_ = results[0];
  return true;
}

void SimilarityTransform3::TransformPose(Eigen::Vector4d* qvec,
                                         Eigen::Vector3d* tvec) const {
  Eigen::Matrix4d inverse;
  inverse.topRows<3>() = Inverse().Matrix();
  inverse.row(3) = Eigen::Vector4d(0, 0, 0, 1);
  const Eigen::Matrix3x4d transformed =
      ComposeProjectionMatrix(*qvec, *tvec) * inverse;
  const double transformed_scale = transformed.col(0).norm();
  *qvec =
      RotationMatrixToQuaternion(transformed.leftCols<3>() / transformed_scale);
  *tvec = transformed.col(3) / transformed_scale;
}

void SimilarityTransform3::ToFile(const std::string& path) const {
  std::ofstream file(path, std::ios::trunc);
  CHECK(file.good()) << path;
  // Ensure that we don't loose any precision by storing in text.
  file.precision(17);
  file << matrix_ << std::endl;
}

SimilarityTransform3 SimilarityTransform3::FromFile(const std::string& path) {
  std::ifstream file(path);
  CHECK(file.good()) << path;

  Eigen::Matrix3x4d matrix;
  for (int i = 0; i < matrix.rows(); ++i) {
    for (int j = 0; j < matrix.cols(); ++j) {
      file >> matrix(i, j);
    }
  }

  return SimilarityTransform3(matrix);
}

}  // namespace colmap