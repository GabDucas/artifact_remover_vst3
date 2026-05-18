#pragma once

#include <Eigen/Dense>

namespace artifact_remover
{

// Row-major matrix for memory layout optimization
using Matrix = Eigen::Matrix<
    double,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::RowMajor>;
// using Matrix = Eigen::MatrixXd; // Default is column-major, but we will use RowMajor for better cache performance with MKL SVD

// Column-major matrix for MKL compatibility
using ColMajorMatrix = Eigen::Matrix<
    double,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::ColMajor>;

// Standard Eigen vector (column vector)
using Vector = Eigen::VectorXd;

} // namespace artifact_remover
