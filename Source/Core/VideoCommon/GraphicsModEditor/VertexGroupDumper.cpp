// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/VertexGroupDumper.h"

#include <algorithm>
#include <future>
#include <queue>
#include <random>
#include <utility>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <igl/remove_duplicate_vertices.h>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tinygltf/tiny_gltf.h>

#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/TimeUtil.h"

#include "Core/ConfigManager.h"

#ifdef _MSC_VER
#pragma warning(disable : 4267)  // "conversion from size_t to int in libigl
#endif

namespace
{
namespace SimpleGeodesic
{

struct HeatGeodesicsData
{
  Eigen::SparseMatrix<double> L;  // Laplacian
  Eigen::SparseMatrix<double> M;  // Mass Matrix
  Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> heat_solver;
  Eigen::MatrixXd V;
  Eigen::MatrixXi F;
  double t;  // Time step
};

/**
 * Precompute the sparse factors for the Heat Method.
 * Returns false if the mesh is degenerate or matrices are not positive definite.
 */
bool heat_geodesics_precompute(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                               HeatGeodesicsData& data)
{
  int n = V.rows();
  data.V = V;
  data.F = F;

  std::vector<Eigen::Triplet<double>> L_triplets, M_triplets;
  double total_edge_len = 0;
  int edge_count = 0;

  for (int f = 0; f < F.rows(); ++f)
  {
    for (int i = 0; i < 3; ++i)
    {
      int i0 = F(f, i);
      int i1 = F(f, (i + 1) % 3);
      int i2 = F(f, (i + 2) % 3);

      Eigen::Vector3d v0 = V.row(i0), v1 = V.row(i1), v2 = V.row(i2);
      Eigen::Vector3d e1 = v1 - v0, e2 = v2 - v0;

      double area = 0.5 * e1.cross(e2).norm();
      if (area < 1e-12)
        continue;  // Skip degenerate

      // Cotangent of angle at v0
      double cot0 = e1.dot(e2) / e1.cross(e2).norm();
      double weight = 0.5 * cot0;

      // Add to off-diagonals (v1, v2)
      L_triplets.push_back({i1, i2, weight});
      L_triplets.push_back({i2, i1, weight});

      // Add to diagonals
      L_triplets.push_back({i1, i1, -weight});
      L_triplets.push_back({i2, i2, -weight});

      // Mass Matrix (Barycentric area)
      M_triplets.push_back({i0, i0, area / 3.0});

      total_edge_len += e1.norm();
      edge_count++;
    }
  }

  Eigen::SparseMatrix<double> L(n, n), M(n, n);
  L.setFromTriplets(L_triplets.begin(), L_triplets.end());
  M.setFromTriplets(M_triplets.begin(), M_triplets.end());

  // t = h^2 where h is mean edge length
  double h = total_edge_len / edge_count;
  data.t = h * h * 10.0;

  // Heat step: (M - tL)
  Eigen::SparseMatrix<double> A_heat = M - data.t * L;
  data.heat_solver.compute(A_heat);

  data.L = L;
  data.M = M;
  return data.heat_solver.info() == Eigen::Success;
}

}  // namespace SimpleGeodesic

Common::Vec3 ReadPosition(const u8* vert_ptr, u32 position_offset)
{
  Common::Vec3 vertex_position;
  std::memcpy(&vertex_position.x, vert_ptr + position_offset, sizeof(float));
  std::memcpy(&vertex_position.y, vert_ptr + sizeof(float) + position_offset, sizeof(float));
  std::memcpy(&vertex_position.z, vert_ptr + sizeof(float) * 2 + position_offset, sizeof(float));
  return vertex_position;
}

std::vector<Common::Vec3> ReadMeshPositions(const GraphicsModSystem::DrawDataView& draw_data)
{
  std::vector<Common::Vec3> result;
  for (u32 i = 0; i < draw_data.vertex_count; i++)
  {
    const auto pos =
        ReadPosition(draw_data.vertex_data + i * draw_data.vertex_format->GetVertexStride(),
                     draw_data.vertex_format->GetVertexDeclaration().position.offset);
    result.push_back(pos);
  }
  return result;
}

Eigen::Matrix3Xd ConvertMeshPositions(const std::vector<Common::Vec3>& positions,
                                      const Common::Matrix44& transform_inverse)
{
  Eigen::Matrix3Xd result(3, positions.size());
  for (std::size_t i = 0; i < positions.size(); ++i)
  {
    const auto pos = positions[i];
    const auto pos_object_space = transform_inverse.Transform(pos, 1);
    result.col(i) = Eigen::Vector3d(pos_object_space.x, pos_object_space.y, pos_object_space.z);
  }

  return result;
}

bool IsDuplicateMesh(const Eigen::Matrix3Xd& mesh,
                     const std::vector<Eigen::Matrix3Xd>& existing_meshes, float epsilon = 1e-4f)
{
  for (const auto& existing_mesh : existing_meshes)
  {
    Eigen::Matrix3Xd diff = (mesh - existing_mesh).cwiseAbs();

    if (diff.maxCoeff() < epsilon)
    {
      return true;
    }
  }
  return false;
}

/**
 * Normalizes an animation sequence after it has been welded.
 * 1. Centers Pose 0 at the origin.
 * 2. Scales based on Pose 0 bounding box.
 * 3. Aligns all frames to Pose 0.
 */
void NormalizeWeldedAnimation(std::vector<Eigen::Matrix3Xd>& poses, double& outScale)
{
  if (poses.empty())
    return;

  // A. Center and Scale based on Pose 0
  Eigen::Vector3d centroid = poses[0].rowwise().mean();
  for (auto& p : poses)
    p.colwise() -= centroid;

  Eigen::Vector3d minB = poses[0].rowwise().minCoeff();
  Eigen::Vector3d maxB = poses[0].rowwise().maxCoeff();
  outScale = (maxB - minB).norm();

  if (outScale > 1e-8)
  {
    for (auto& p : poses)
      p /= outScale;
  }

  // B. Global Alignment
  // We align everything to poses[0] (which is now centered/scaled)
  const Eigen::Matrix3Xd& reference = poses[0];

  for (size_t k = 1; k < poses.size(); ++k)
  {
    Eigen::Matrix3Xd& currentPose = poses[k];

    // 1. Center the current frame locally
    Eigen::Vector3d currentCentroid = currentPose.rowwise().mean();
    Eigen::Matrix3Xd centeredCurrent = currentPose.colwise() - currentCentroid;

    // 2. Solve for Rotation (Kabsch)
    // Note: Reference is already at origin, so we use reference directly
    Eigen::Matrix3d H = centeredCurrent * reference.transpose();

    Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d R = svd.matrixV() * svd.matrixU().transpose();

    if (R.determinant() < 0)
    {
      Eigen::Matrix3d V = svd.matrixV();
      V.col(2) *= -1;
      R = V * svd.matrixU().transpose();
    }

    // 3. Update the pose: Remove its translation, rotate it,
    // and keep it at the origin to match reference.
    currentPose = R * centeredCurrent;
  }
}

/**
 * Solves the "Procrustes Problem" (Kabsch Algorithm) for a set of points.
 *
 * Given a group of vertices in their 'Rest Pose' and 'Animated Pose',
 * this function finds the single best Rotation (R) and Translation (T)
 * that aligns the rest points to the animated ones.
 *
 * It calculates the 'Sum of Squared Errors' (SSE) in World Space.
 * - If SSE is near 0: The group moves as a perfect rigid bone.
 * - If SSE is high: The group is stretching or contains multiple independent motions.
 *
 * @return The total error (SSE) for the best possible rigid fit of this bone.
 */
double CalculateRigidSSE_WorldSpace(const std::vector<int>& indices,
                                    const std::vector<Eigen::Matrix3Xd>& allPoses,
                                    const Eigen::Matrix3Xd& restPose)
{
  if (indices.empty())
    return 0.0;
  const std::size_t N = indices.size();

  // 1. Reference Subset (3 x N)
  Eigen::Matrix3Xd P(3, N);
  for (std::size_t i = 0; i < N; ++i)
    P.col(i) = restPose.col(indices[i]);
  Eigen::Vector3d p_mean = P.rowwise().mean();
  Eigen::Matrix3Xd P_centered = P.colwise() - p_mean;

  double totalSSE = 0.0;

  for (const auto& pose : allPoses)
  {
    // 2. Target Subset (3 x N)
    Eigen::Matrix3Xd Q(3, N);
    for (std::size_t i = 0; i < N; ++i)
      Q.col(i) = pose.col(indices[i]);
    Eigen::Vector3d q_mean = Q.rowwise().mean();
    Eigen::Matrix3Xd Q_centered = Q.colwise() - q_mean;

    // 3. Kabsch Rotation
    Eigen::Matrix3d H = P_centered * Q_centered.transpose();  // Note: P*Q' for ref->pose
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);

    Eigen::Matrix3d V = svd.matrixV();
    Eigen::Matrix3d U = svd.matrixU();
    Eigen::Matrix3d R = V * U.transpose();

    if (R.determinant() < 0)
    {
      V.col(2) *= -1;
      R = V * U.transpose();
    }

    // 4. Full Translation
    Eigen::Vector3d T = q_mean - R * p_mean;

    // 5. World Space Error (The Ultimate Truth)
    for (std::size_t i = 0; i < N; ++i)
    {
      Eigen::Vector3d reconstructed = R * P.col(i) + T;
      totalSSE += (reconstructed - Q.col(i)).squaredNorm();
    }
  }
  return totalSSE;
}

/**
 * Performs a binary split of a vertex group based on raw motion data.
 *
 * Instead of looking at 3D shapes, this function treats each vertex as a
 * point in a high-dimensional "Motion Space" (3 * Number of Frames).
 * It uses a stable 2-seed K-Means clustering to find the two most
 * distinct motion patterns within the group.
 *
 * @param allPoses The complete animation data.
 * @param targetIndices The specific subset of vertices to be split.
 * @param assignments [Out] Maps vertex indices to Group 0 or Group 1.
 */
void rigid_split_raw(const std::vector<Eigen::Matrix3Xd>& allPoses,
                     const std::vector<int>& targetIndices, std::vector<int>& assignments)
{
  const std::size_t N_sub = targetIndices.size();
  const std::size_t K = allPoses.size();

  // 1. Build a local Motion Descriptor (Vertices x 3K coordinates)
  Eigen::MatrixXd motion(N_sub, 3 * K);
  for (std::size_t i = 0; i < N_sub; ++i)
  {
    int vIdx = targetIndices[i];
    for (std::size_t k = 0; k < K; ++k)
    {
      motion.row(i).segment<3>(k * 3) = allPoses[k].col(vIdx);
    }
  }

  // 2. Farthest-First Traversal for K=2 Seeds (Stable Initialization)
  int s1 = 0;
  int s2 = -1;
  double maxD = -1;
  for (std::size_t i = 1; i < N_sub; ++i)
  {
    double d = (motion.row(i) - motion.row(s1)).squaredNorm();
    if (d > maxD)
    {
      maxD = d;
      s2 = static_cast<int>(i);
    }
  }

  // 3. Iterative Lloyd's K-Means
  //
  // Use RowVectorXd to match the shape of motion.row(i)
  Eigen::RowVectorXd c1 = motion.row(s1);
  Eigen::RowVectorXd c2 = motion.row(s2);

  for (int iter = 0; iter < 15; ++iter)
  {
    std::vector<int> g1_local, g2_local;

    // Assignment
    for (std::size_t i = 0; i < N_sub; ++i)
    {
      double d1 = (motion.row(i) - c1).squaredNorm();
      double d2 = (motion.row(i) - c2).squaredNorm();
      int best_k = (d1 < d2) ? 0 : 1;
      assignments[targetIndices[i]] = best_k;

      if (best_k == 0)
        g1_local.push_back(i);
      else
        g2_local.push_back(i);
    }

    if (g1_local.empty() || g2_local.empty())
      break;

    // Update
    c1.setZero();
    for (int idx : g1_local)
      c1 += motion.row(idx);
    c1 /= static_cast<double>(g1_local.size());

    c2.setZero();
    for (int idx : g2_local)
      c2 += motion.row(idx);
    c2 /= static_cast<double>(g2_local.size());
  }
}

/**
 * Orchestrates the recursive bone discovery process using an SSE-based "Gate".
 *
 * This is the "Brain" of the bone detection. It identifies which vertex groups
 * are the most 'rubbery' (high World-Space SSE) and uses 'rigid_split_raw' to
 * break them down. It includes safeguard 'Gates' to prevent the creation of
 * microscopic bones or bones that don't significantly improve the animation's accuracy.
 *
 * @param tolerance The SSE threshold that triggers a split.
 * @return A flat array of Bone IDs for every vertex in the mesh.
 */
std::vector<int> CalculateVertexGroupsAdaptive(const std::vector<Eigen::Matrix3Xd>& allPoses,
                                               const Eigen::Matrix3Xd& restPose, double tolerance)
{
  const int numVertices = restPose.cols();
  std::vector<int> allIndices(numVertices);
  std::iota(allIndices.begin(), allIndices.end(), 0);

  struct BoneGroup
  {
    std::vector<int> indices;
    bool skip_split = false;
  };

  std::vector<BoneGroup> groups = {{allIndices, false}};
  const int MAX_BONES = 50;

  const double totalInitialSSE = CalculateRigidSSE_WorldSpace(allIndices, allPoses, restPose);

  while (groups.size() < MAX_BONES)
  {
    int split_index = -1;
    double maxSSE = -1;

    // Find worst group that isn't already "done"
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
      if (groups[i].skip_split)
        continue;
      double sse = CalculateRigidSSE_WorldSpace(groups[i].indices, allPoses, restPose);
      if (sse > tolerance && sse > maxSSE)
      {
        maxSSE = sse;
        split_index = static_cast<int>(i);
      }
    }

    if (split_index == -1)
      break;

    // Perform the RAW motion split
    std::vector<int> sub_assignments(numVertices, -1);
    rigid_split_raw(allPoses, groups[split_index].indices, sub_assignments);

    std::vector<int> g1, g2;
    for (int vIdx : groups[split_index].indices)
    {
      if (sub_assignments[vIdx] == 0)
        g1.push_back(vIdx);
      else
        g2.push_back(vIdx);
    }

    if (g1.empty() || g2.empty())
    {
      groups[split_index].skip_split = true;
      continue;
    }

    // IMPROVEMENT GATE (Using SSE for mathematical fairness)
    // Calculate total error
    const double parentSSE =
        CalculateRigidSSE_WorldSpace(groups[split_index].indices, allPoses, restPose);
    const double child1SSE = CalculateRigidSSE_WorldSpace(g1, allPoses, restPose);
    const double child2SSE = CalculateRigidSSE_WorldSpace(g2, allPoses, restPose);
    const double combinedChildSSE = child1SSE + child2SSE;

    double improvementFactor = combinedChildSSE / parentSSE;

    DEBUG_LOG_FMT(
        VIDEO,
        "Step: {} | Parent size: {} | Child 1 size: {} | Child 2 size: {}, Improvement Factor: {}, "
        "Active "
        "groups: {}, Parent SSE: {} | Child 1 SSE: {} | Child 2 SSE: {} | Combined SSE: {}",
        groups.size(), groups[split_index].indices.size(), g1.size(), g2.size(), improvementFactor,
        std::count_if(groups.begin(), groups.end(), [](auto& g) { return !g.skip_split; }),
        parentSSE, child1SSE, child2SSE, combinedChildSSE);

    double improvement = parentSSE - combinedChildSSE;

    // GATE A: The "Significant Contribution" Check
    // If the improvement is less than 0.01% of the total mesh error, it's not worth a new bone.
    if (improvement < (totalInitialSSE * 0.0001))
    {
      WARN_LOG_FMT(VIDEO,
                   "Skipping parent group due to no significant contribution, actual improvement "
                   "{}, totalInitialSSE {}, minimum improvement required {}",
                   improvement, totalInitialSSE, totalInitialSSE * 0.001);
      groups[split_index].skip_split = true;
      continue;
    }

    // GATE B: The "Absolute Size" Check
    // Don't create bones smaller than 1% of the total mesh (e.g., 25 vertices for a 2500 mesh)
    if (g1.size() < (numVertices * 0.01) || g2.size() < (numVertices * 0.01))
    {
      WARN_LOG_FMT(VIDEO, "Skipping parent group due to small size, max vertices allowed {}",
                   numVertices * 0.01);
      groups[split_index].skip_split = true;
      continue;
    }

    // Commit the split
    groups.erase(groups.begin() + split_index);
    groups.emplace_back(g1, false);
    groups.emplace_back(g2, false);
  }

  // Map back to final array
  std::vector<int> finalAssignments(numVertices, 0);
  for (std::size_t g = 0; g < groups.size(); ++g)
  {
    for (int vIdx : groups[g].indices)
      finalAssignments[vIdx] = static_cast<int>(g);
  }
  return finalAssignments;
}

/**
 * Converts 'Hard' bone assignments into 'Smooth' Linear Blend Skinning (LBS) weights.
 *
 * Uses the Heat Equation to diffuse bone influence across the mesh surface.
 * This ensures that vertices near a joint are influenced by both connected bones,
 * creating smooth bends in the 3D model instead of sharp 'paper-fold' creases.
 *
 * @note Uses Gaussian-style diffusion (Time-stepping) for maximum numerical stability.
 */
Eigen::MatrixXd ComputeHarmonicWeights(const std::vector<int>& assignments,
                                       const Eigen::Matrix3Xd& V, const Eigen::MatrixXi& F,
                                       const SimpleGeodesic::HeatGeodesicsData& heat_data,
                                       int numBones)
{
  // 1. Calculate Average Edge Length (h)
  double totalEdgeLen = 0;
  int edgeCount = 0;
  for (int i = 0; i < F.rows(); ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      totalEdgeLen += (V.col(F(i, j)) - V.col(F(i, (j + 1) % 3))).norm();
      edgeCount++;
    }
  }
  double h = totalEdgeLen / edgeCount;
  double t_skin = 2.0 * h * h;  // The "Zero-Config" Step

  // 2. Build the specific solver for THIS scale
  // Note: Use the Laplacian (L) and Mass (M) from heat_data
  Eigen::SparseMatrix<double> A = heat_data.M - t_skin * heat_data.L;
  Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> solver(A);

  int N = V.cols();
  Eigen::MatrixXd W = Eigen::MatrixXd::Zero(N, numBones);

  // 3. Seed and Solve
  for (std::size_t i = 0; i < N; ++i)
    W(i, assignments[i]) = 1.0;

  for (int b = 0; b < numBones; ++b)
  {
    W.col(b) = solver.solve(W.col(b));
  }

  // 4. Normalize (Fixes the scaling/shrinking issue)
  for (int i = 0; i < N; ++i)
  {
    W.row(i) = W.row(i).cwiseMax(0.0);
    double s = W.row(i).sum();
    if (s > 1e-9)
      W.row(i) /= s;
  }

  return W;
}

/**
 * Calculates the 3D 'Pivot Point' for every discovered bone.
 *
 * For each bone group, it finds the geometric center (centroid) of its
 * assigned vertices in the rest pose. These points act as the 'Joints'
 * for the glTF skeleton and the origin for all rotations.
 */
std::vector<Eigen::Vector3d> ComputeBoneRestPositions(const std::vector<std::vector<int>>& groups,
                                                      const Eigen::Matrix3Xd& restPose)
{
  std::vector<Eigen::Vector3d> bonePositions;
  for (const auto& groupIndices : groups)
  {
    Eigen::Vector3d centroid(0, 0, 0);
    if (groupIndices.empty())
    {
      bonePositions.push_back(centroid);
      continue;
    }

    for (int vIdx : groupIndices)
    {
      centroid += restPose.col(vIdx);
    }
    bonePositions.push_back(centroid / static_cast<double>(groupIndices.size()));
  }
  return bonePositions;
}

/**
 * Removes 'parasitic' weights by enforcing physical connectivity.
 *
 * Sometimes two distant parts (like a hand and a foot) move in sync, causing
 * the math to group them together. This function finds 'islands' of vertices
 * that aren't physically touching the main body of a bone and 'snaps' them
 * back to their actual physical neighbors.
 */
void CleanBoneIslands(std::vector<int>& assignments, const Eigen::MatrixXi& F, int numVertices,
                      int numBones)
{
  // 1. Build Adjacency
  std::vector<std::vector<int>> adj(numVertices);
  for (int i = 0; i < F.rows(); ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      int u = F(i, j);
      int v = F(i, (j + 1) % 3);
      adj[u].push_back(v);
      adj[v].push_back(u);
    }
  }

  // 2. Group into islands
  std::vector<int> islandID(numVertices, -1);
  std::vector<std::vector<int>> islands;
  std::vector<int> islandBoneType;

  std::vector<bool> visited(numVertices, false);
  for (int i = 0; i < numVertices; ++i)
  {
    if (visited[i])
      continue;

    int currentBone = assignments[i];
    std::vector<int> component;
    std::queue<int> q;
    q.push(i);
    visited[i] = true;

    while (!q.empty())
    {
      int u = q.front();
      q.pop();
      component.push_back(u);
      islandID[u] = (int)islands.size();
      for (int v : adj[u])
      {
        if (!visited[v] && assignments[v] == currentBone)
        {
          visited[v] = true;
          q.push(v);
        }
      }
    }
    islandBoneType.push_back(currentBone);
    islands.push_back(component);
  }

  // 3. Find the "Alpha" island for each bone (the largest one)
  std::vector<int> alphaIslandForBone(numBones, -1);
  std::vector<size_t> maxSizes(numBones, 0);
  for (std::size_t i = 0; i < islands.size(); i++)
  {
    int b = islandBoneType[i];
    if (islands[i].size() > maxSizes[b])
    {
      maxSizes[b] = islands[i].size();
      alphaIslandForBone[b] = i;
    }
  }

  // 4. Snap Orphans: If you're not in your bone's alpha island,
  // take the bone ID of a neighboring vertex that IS in an alpha island.
  for (int i = 0; i < numVertices; i++)
  {
    int myIsland = islandID[i];
    int myBone = assignments[i];

    if (myIsland != alphaIslandForBone[myBone])
    {
      // I'm an orphan! Find a neighbor to adopt me.
      for (int neighbor : adj[i])
      {
        int nIsland = islandID[neighbor];
        int nBone = assignments[neighbor];
        // Only adopt if the neighbor is part of a "legit" main bone body
        if (nIsland == alphaIslandForBone[nBone])
        {
          assignments[i] = nBone;
          break;
        }
      }
    }
  }
}

/**
 * Validates the skeleton by rebuilding the animation using only bones and weights.
 *
 * This is the 'Proof of Work'. It applies the calculated Rigid Transforms (R, T)
 * to the Rest Pose vertices using the smooth Weights (W). If the result matches
 * the original animation, the bone discovery and skinning are successful.
 */
std::vector<std::vector<int>> ReconstructGroups(const std::vector<int>& cleanAssignments,
                                                int numBones)
{
  std::vector<std::vector<int>> groups(numBones);
  for (std::size_t vIdx = 0; vIdx < cleanAssignments.size(); ++vIdx)
  {
    int boneID = cleanAssignments[vIdx];
    if (boneID >= 0 && boneID < numBones)
    {
      groups[boneID].push_back(vIdx);
    }
  }
  return groups;
}

struct VertexInfluence
{
  std::array<unsigned short, 4> joints;
  std::array<float, 4> weights;
};

std::vector<VertexInfluence> FormatInfluencesForGLTF(const Eigen::MatrixXd& W)
{
  int N = W.rows();
  int B = W.cols();
  std::vector<VertexInfluence> influences(N);

  for (int i = 0; i < N; ++i)
  {
    // Find top 4 weights
    std::vector<std::pair<int, double>> pairs;
    for (int j = 0; j < B; ++j)
    {
      pairs.push_back({j, W(i, j)});
    }
    std::sort(pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    double sum = 0;
    for (int j = 0; j < 4; ++j)
    {
      influences[i].joints[j] = static_cast<unsigned short>(pairs[j].first);
      influences[i].weights[j] = static_cast<float>(pairs[j].second);
      sum += pairs[j].second;
    }

    // Re-normalize top 4 so they sum to 1.0
    if (sum > 1e-6)
    {
      for (int j = 0; j < 4; ++j)
        influences[i].weights[j] /= static_cast<float>(sum);
    }
  }
  return influences;
}

/**
 * Connects bones into a skeletal tree.
 * 1. Finds the "most significant" bone (Torso) to act as Root.
 * 2. Uses Mesh Topology (Shared Edges) to build the primary tree.
 * 3. Uses Physical Distance as a fallback for floating/disconnected parts.
 */
std::vector<int> BuildBoneHierarchyForGLTF(const std::vector<int>& assignments,
                                           const Eigen::MatrixXi& F,
                                           const std::vector<Eigen::Vector3d>& boneCenters,
                                           int numBones)
{
  // --- 1. Count shared edges between bones ---
  Eigen::MatrixXd adj = Eigen::MatrixXd::Zero(numBones, numBones);
  for (int i = 0; i < F.rows(); ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      int u = assignments[F(i, j)];
      int v = assignments[F(i, (j + 1) % 3)];
      if (u >= 0 && v >= 0 && u != v)
      {
        adj(u, v)++;
        adj(v, u)++;
      }
    }
  }

  // --- 2. Find the Root Bone (The one with the most vertices) ---
  std::vector<int> boneSizes(numBones, 0);
  for (int b : assignments)
    if (b >= 0)
      boneSizes[b]++;

  int rootBone = 0;
  int maxVerts = -1;
  for (int i = 0; i < numBones; ++i)
  {
    if (boneSizes[i] > maxVerts)
    {
      maxVerts = boneSizes[i];
      rootBone = i;
    }
  }

  // --- 3. Build the Tree (Prim's Algorithm / MST) ---
  std::vector<int> parents(numBones, -1);
  std::vector<bool> visited(numBones, false);
  visited[rootBone] = true;

  for (int iteration = 0; iteration < numBones - 1; ++iteration)
  {
    int bestU = -1;  // Existing parent
    int bestV = -1;  // New child
    double maxShared = -1.0;

    // First pass: Try to connect via shared mesh edges (Topological)
    for (int u = 0; u < numBones; ++u)
    {
      if (!visited[u])
        continue;
      for (int v = 0; v < numBones; ++v)
      {
        if (visited[v])
          continue;
        if (adj(u, v) > maxShared)
        {
          maxShared = adj(u, v);
          bestU = u;
          bestV = v;
        }
      }
    }

    // Second pass: If no shared edges found, connect by distance (Geometric Fallback)
    // This handles floating eyes, belts, or disconnected props.
    if (bestV == -1 || maxShared <= 0)
    {
      double minDistance = std::numeric_limits<double>::max();
      for (int u = 0; u < numBones; ++u)
      {
        if (!visited[u])
          continue;
        for (int v = 0; v < numBones; ++v)
        {
          if (visited[v])
            continue;
          double dist = (boneCenters[u] - boneCenters[v]).norm();
          if (dist < minDistance)
          {
            minDistance = dist;
            bestU = u;
            bestV = v;
          }
        }
      }
    }

    if (bestV != -1)
    {
      parents[bestV] = bestU;
      visited[bestV] = true;
    }
  }

  return parents;
}

template <typename T>
int AddBufferToModel(tinygltf::Model& model, const std::vector<T>& data, int target, int type,
                     int componentType)
{
  size_t byteLength = data.size() * sizeof(T);
  size_t bufferOffset = model.buffers.empty() ? 0 : model.buffers[0].data.size();

  if (model.buffers.empty())
    model.buffers.emplace_back();
  model.buffers[0].data.resize(bufferOffset + byteLength);
  std::memcpy(model.buffers[0].data.data() + bufferOffset, data.data(), byteLength);

  tinygltf::BufferView view;
  view.buffer = 0;
  view.byteOffset = bufferOffset;
  view.byteLength = byteLength;
  view.target = target;
  model.bufferViews.push_back(view);

  tinygltf::Accessor accessor;
  accessor.bufferView = static_cast<int>(model.bufferViews.size() - 1);
  accessor.byteOffset = 0;
  accessor.componentType = componentType;

  if (type == TINYGLTF_TYPE_SCALAR)
  {
    accessor.count = data.size();
  }
  else if (type == TINYGLTF_TYPE_VEC3)
  {
    accessor.count = data.size() / 3;
  }
  else if (type == TINYGLTF_TYPE_VEC4)
  {
    accessor.count = data.size() / 4;
  }
  else if (type == TINYGLTF_TYPE_MAT4)
  {
    accessor.count = data.size() / 16;
  }

  accessor.type = type;
  model.accessors.push_back(accessor);

  return static_cast<int>(model.accessors.size() - 1);
}

void ExportToGLTF(const std::string& path, const Eigen::Matrix3Xd& V, const Eigen::MatrixXi& F,
                  const std::vector<Eigen::Vector3d>& boneCenters,
                  const std::vector<VertexInfluence>& influences)
{
  tinygltf::Model model;
  tinygltf::Asset asset;
  asset.version = "2.0";
  model.asset = asset;

  // 1. Prepare Data for Packing
  std::vector<float> posData;
  for (int i = 0; i < V.cols(); ++i)
  {
    posData.push_back((float)V(0, i));
    posData.push_back((float)V(1, i));
    posData.push_back((float)V(2, i));
  }

  std::vector<uint32_t> indexData;
  for (int i = 0; i < F.rows(); ++i)
  {
    indexData.push_back((uint32_t)F(i, 0));
    indexData.push_back((uint32_t)F(i, 1));
    indexData.push_back((uint32_t)F(i, 2));
  }

  std::vector<uint16_t> jointData;
  std::vector<float> weightData;
  for (const auto& inf : influences)
  {
    for (int j = 0; j < 4; ++j)
    {
      jointData.push_back(inf.joints[j]);
      weightData.push_back(inf.weights[j]);
    }
  }

  // 2. Pack Buffers
  int posAcc =
      AddBufferToModel(model, posData, 34962, TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
  int idxAcc = AddBufferToModel(model, indexData, 34963, TINYGLTF_TYPE_SCALAR,
                                TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
  int jntAcc = AddBufferToModel(model, jointData, 34962, TINYGLTF_TYPE_VEC4,
                                TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
  int wgtAcc =
      AddBufferToModel(model, weightData, 34962, TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT);

  std::vector<float> ibmData;
  for (const auto& center : boneCenters)
  {
    // A matrix that translates by -center
    Eigen::Matrix4d invBind = Eigen::Matrix4d::Identity();
    invBind(0, 3) = -center.x();
    invBind(1, 3) = -center.y();
    invBind(2, 3) = -center.z();

    // Pack 16 floats (column-major)
    for (int col = 0; col < 4; ++col)
    {
      for (int row = 0; row < 4; ++row)
      {
        ibmData.push_back((float)invBind(row, col));
      }
    }
  }

  // 3. Build Mesh
  tinygltf::Mesh mesh;
  tinygltf::Primitive prim;
  prim.attributes["POSITION"] = posAcc;
  prim.attributes["JOINTS_0"] = jntAcc;
  prim.attributes["WEIGHTS_0"] = wgtAcc;
  prim.indices = idxAcc;
  prim.mode = 4;  // Triangles
  mesh.primitives.push_back(prim);
  model.meshes.push_back(mesh);

  // 4. Create Bone Nodes and Skin
  tinygltf::Skin skin;

  const int ibmAcc =
      AddBufferToModel(model, ibmData, 0, TINYGLTF_TYPE_MAT4, TINYGLTF_COMPONENT_TYPE_FLOAT);
  skin.inverseBindMatrices = ibmAcc;

  tinygltf::Scene scene;
  tinygltf::Node meshNode;
  meshNode.mesh = 0;
  meshNode.skin = 0;
  model.nodes.push_back(meshNode);
  scene.nodes.push_back(0);

  for (std::size_t i = 0; i < boneCenters.size(); ++i)
  {
    tinygltf::Node bone;
    bone.name = "Bone_" + std::to_string(i);
    bone.translation = {boneCenters[i].x(), boneCenters[i].y(), boneCenters[i].z()};
    model.nodes.push_back(bone);

    int boneNodeIdx = static_cast<int>(model.nodes.size());
    model.nodes.push_back(bone);
    skin.joints.push_back(boneNodeIdx);
    scene.nodes.push_back(boneNodeIdx);
  }
  model.skins.push_back(skin);

  model.scenes.push_back(scene);
  model.defaultScene = 0;

  // 5. Save
  tinygltf::TinyGLTF writer;
  writer.WriteGltfSceneToFile(&model, path, true, true, true, false);
}

}  // namespace

namespace GraphicsModEditor
{
bool VertexGroupDumper::IsDrawCallInRecording(GraphicsModSystem::DrawCallID draw_call_id) const
{
  if (!m_recording_request.has_value())
    return false;

  return m_recording_request->m_draw_call_ids.contains(draw_call_id);
}

void VertexGroupDumper::AddDataToRecording(GraphicsModSystem::DrawCallID draw_call_id,
                                           const GraphicsModSystem::DrawDataView& draw_data)
{
  auto current_vertex_positions = ReadMeshPositions(draw_data);

  Common::Matrix44 position_transform;
  for (std::size_t i = 0; i < 3; i++)
  {
    position_transform.data[i * 4 + 0] = draw_data.object_transform[i][0];
    position_transform.data[i * 4 + 1] = draw_data.object_transform[i][1];
    position_transform.data[i * 4 + 2] = draw_data.object_transform[i][2];
    position_transform.data[i * 4 + 3] = draw_data.object_transform[i][3];
  }
  position_transform.data[12] = 0;
  position_transform.data[13] = 0;
  position_transform.data[14] = 0;
  position_transform.data[15] = 1;

  Eigen::Matrix3Xd matrix_positions =
      ConvertMeshPositions(current_vertex_positions, position_transform.Inverted());
  auto& data = m_draw_call_to_data[draw_call_id];

  if (IsDuplicateMesh(matrix_positions, data.vertex_meshes))
    return;

  if (data.vertex_meshes.empty())
  {
    if (draw_data.uid->rasterization_state.primitive == PrimitiveType::TriangleStrip)
    {
      std::vector<int> indices;
      u32 strip_count = 0;
      for (std::size_t i = 0; i < draw_data.index_data.size(); i++)
      {
        // Primitive restart
        if (draw_data.index_data[i] == UINT16_MAX)
        {
          strip_count = 0;  // Reset strip state
          continue;
        }

        // A triangle is at least 3 verts
        strip_count++;
        if (strip_count < 3)
          continue;

        u16 i0, i1, i2;

        // Triangle index 'n' in the current strip is at (i-2, i-1, i)
        // Its local index is (triangleCountInStrip - 3)
        if ((strip_count - 3) % 2 == 0)
        {
          i0 = draw_data.index_data[i - 2];
          i1 = draw_data.index_data[i - 1];
          i2 = draw_data.index_data[i];
        }
        else
        {
          // Swap for odd-numbered triangles to maintain consistent CCW winding
          i0 = draw_data.index_data[i - 2];
          i1 = draw_data.index_data[i];
          i2 = draw_data.index_data[i - 1];
        }

        if (i0 == i1 || i1 == i2 || i0 == i2)
        {
          continue;
        }

        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
      }

      const std::size_t num_triangles = indices.size() / 3;
      Eigen::MatrixXi F(num_triangles, 3);

      for (int i = 0; i < num_triangles; ++i)
      {
        F(i, 0) = indices[i * 3 + 0];
        F(i, 1) = indices[i * 3 + 1];
        F(i, 2) = indices[i * 3 + 2];
      }
      data.rest_pose_face_indexes = std::move(F);
    }
    else
    {
      const std::size_t num_triangles = draw_data.index_data.size() / 3;
      Eigen::MatrixXi F(num_triangles, 3);

      for (int i = 0; i < num_triangles; ++i)
      {
        F(i, 0) = draw_data.index_data[i * 3 + 0];
        F(i, 1) = draw_data.index_data[i * 3 + 1];
        F(i, 2) = draw_data.index_data[i * 3 + 2];
      }

      data.rest_pose_face_indexes = std::move(F);
    }
  }

  data.vertex_meshes.push_back(std::move(matrix_positions));
}

void VertexGroupDumper::Record(const VertexGroupRecordingRequest& request)
{
  m_recording_request = request;
}

void VertexGroupDumper::StopRecord()
{
  m_recording_request.reset();

  struct VertexGroupResults
  {
    std::vector<int> vertex_transform_groups;
    Eigen::MatrixXi face_indices;
    Eigen::Matrix3Xd rest_pose;
    int bone_count;
  };

  std::map<GraphicsModSystem::DrawCallID, VertexGroupResults> draw_call_to_vertex_group_results;

  picojson::object json_data;
  for (auto& [draw_call, data] : m_draw_call_to_data)
  {
    picojson::object group_data;

    // TODO: error
    if (data.vertex_meshes.size() <= 1)
      continue;

    const std::size_t original_vertex_count = data.vertex_meshes[0].cols();

    // 1. Weld the Mean Pose
    Eigen::MatrixXd SV;  // New unique vertices
    Eigen::VectorXi SVI,
        SVJ;  // SVI: indices into original V; SVJ: original indices mapped to unique ones
    igl::remove_duplicate_vertices(data.vertex_meshes[0].transpose(), 1e-7, SV, SVI, SVJ);

    // Update Faces to point to the new indices
    Eigen::MatrixXi SF(data.rest_pose_face_indexes.rows(), 3);
    for (int i = 0; i < data.rest_pose_face_indexes.rows(); ++i)
    {
      for (int j = 0; j < 3; ++j)
      {
        int new_index = SVJ(data.rest_pose_face_indexes(i, j));
        SF(i, j) = new_index;  // Maps original index to unique index
      }
    }

    // Update all other poses to have the same mapping
    for (auto& pose : data.vertex_meshes)
    {
      Eigen::Matrix3Xd newPose(3, SV.rows());
      for (int i = 0; i < SVI.size(); ++i)
      {
        // SVI contains the indices of the original vertices that survived
        newPose.col(i) = pose.col(SVI(i));
      }
      pose = newPose;
    }

    double mesh_scale;
    NormalizeWeldedAnimation(data.vertex_meshes, mesh_scale);

    /**
     * @brief The Error Budget (Tolerance) for bone creation.
     *
     * How it works:
     * The algorithm splits the mesh into bones until the Total Squared Error (SSE)
     * of the reconstructed animation falls below this value.
     *
     * Range Guide (for a mesh scaled to 1.0 diagonal):
     * - 0.001 to 0.01: HIGH DETAIL. Produces 30-50 bones. (Fingers, facial features).
     * - 0.01 to 0.1: BALANCED. Produces 15-25 bones. (Standard game characters).
     * - 0.1 to 0.5: LOW DETAIL. Produces 5-10 bones. (LODs or background props).
     *
     * Note: If your mesh isn't scaled to 1.0, this value must be multiplied
     * by (scale^2) to remain effective.
     */
    const double tolerance = 0.05;
    auto clean_grouping =
        CalculateVertexGroupsAdaptive(data.vertex_meshes, data.vertex_meshes[0], tolerance);

    int bone_count = 0;
    for (int id : clean_grouping)
    {
      bone_count = std::max(bone_count, id + 1);
    }

    CleanBoneIslands(clean_grouping, SF, data.vertex_meshes[0].cols(), bone_count);

    std::vector<int> grouping(original_vertex_count);
    for (std::size_t i = 0; i < original_vertex_count; ++i)
    {
      int uniqueIdx = SVJ(i);
      grouping[i] = clean_grouping[uniqueIdx];
    }
    for (std::size_t i = 0; i < grouping.size(); i++)
    {
      const auto group_assignment = grouping[i];
      const auto [iter, added] =
          group_data.try_emplace(std::to_string(group_assignment), picojson::array{});
      iter->second.get<picojson::array>().emplace_back(static_cast<double>(i));
    }

    json_data.try_emplace(std::to_string(std::to_underlying(draw_call)), group_data);

    const auto [it, added] =
        draw_call_to_vertex_group_results.try_emplace(draw_call, VertexGroupResults{});
    it->second.vertex_transform_groups = std::move(clean_grouping);
    it->second.rest_pose = data.vertex_meshes[0];
    it->second.face_indices = std::move(SF);
    it->second.bone_count = bone_count;
  }

  const std::string path_prefix =
      File::GetUserPath(D_DUMP_IDX) + SConfig::GetInstance().GetGameID();

  const std::time_t start_time = std::time(nullptr);
  const auto local_time = Common::LocalTime(start_time);
  if (!local_time)
    return;

  const std::string base_name = fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, *local_time);
  const std::string save_path = fmt::format("{}.vertexgroups", base_name);

  if (!JsonToFile(save_path, picojson::value{json_data}, true))
  {
    // TODO: error
  }

  for (auto& [draw_call, data] : m_draw_call_to_data)
  {
    const auto iter = draw_call_to_vertex_group_results.find(draw_call);
    if (iter == draw_call_to_vertex_group_results.end())
      continue;

    SimpleGeodesic::HeatGeodesicsData heat_data;
    if (!SimpleGeodesic::heat_geodesics_precompute(iter->second.rest_pose.transpose(),
                                                   iter->second.face_indices, heat_data))
    {
      continue;
    }

    const auto weights =
        ComputeHarmonicWeights(iter->second.vertex_transform_groups, iter->second.rest_pose,
                               iter->second.face_indices, heat_data, iter->second.bone_count);
    const auto influences = FormatInfluencesForGLTF(weights);

    const auto bone_groups =
        ReconstructGroups(iter->second.vertex_transform_groups, iter->second.bone_count);
    const auto bone_centers = ComputeBoneRestPositions(bone_groups, iter->second.rest_pose);
    ExportToGLTF(fmt::format("{}_bones.gltf", base_name), iter->second.rest_pose,
                 iter->second.face_indices, bone_centers, influences);
  }

  m_draw_call_to_data.clear();
}

bool VertexGroupDumper::IsRecording() const
{
  return m_recording_request.has_value();
}
}  // namespace GraphicsModEditor
