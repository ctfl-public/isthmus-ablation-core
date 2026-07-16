#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string read_file(const std::filesystem::path &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot read " + path.string());
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::vector<int> read_int_array(const std::string &xml, const std::string &name) {
  const std::string marker = "Name=\"" + name + "\"";
  const std::size_t name_pos = xml.find(marker);
  if (name_pos == std::string::npos) {
    throw std::runtime_error("missing VTU cell array '" + name + "'");
  }
  const std::size_t start = xml.find('>', name_pos);
  if (start == std::string::npos) {
    throw std::runtime_error("malformed VTU cell array '" + name + "'");
  }
  const std::size_t end = xml.find("</DataArray>", start + 1);
  if (end == std::string::npos) {
    throw std::runtime_error("unterminated VTU cell array '" + name + "'");
  }

  std::istringstream values(xml.substr(start + 1, end - start - 1));
  std::vector<int> result;
  int value = 0;
  while (values >> value) {
    result.push_back(value);
  }
  return result;
}

std::size_t expected_sphere_voxels(int n) {
  const double radius = 0.5 * static_cast<double>(n);
  const double radius2 = radius * radius;
  std::size_t count = 0;
  for (int ix = 0; ix < n; ++ix) {
    const double x = static_cast<double>(ix) + 0.5 - radius;
    for (int iy = 0; iy < n; ++iy) {
      const double y = static_cast<double>(iy) + 0.5 - radius;
      for (int iz = 0; iz < n; ++iz) {
        const double z = static_cast<double>(iz) + 0.5 - radius;
        if (x * x + y * y + z * z <= radius2 + 1.0e-12) {
          ++count;
        }
      }
    }
  }
  return count;
}

std::size_t key(int ix, int iy, int iz, int n) {
  return (static_cast<std::size_t>(ix) * static_cast<std::size_t>(n) +
          static_cast<std::size_t>(iy)) *
             static_cast<std::size_t>(n) +
         static_cast<std::size_t>(iz);
}

void check_axis_planes(const std::vector<std::array<int, 3>> &voxels, int n, int axis,
                       const std::string &label) {
  std::vector<int> counts(static_cast<std::size_t>(n), 0);
  for (const auto &voxel : voxels) {
    ++counts[static_cast<std::size_t>(voxel[axis])];
  }
  for (int i = 0; i < n; ++i) {
    if (counts[static_cast<std::size_t>(i)] == 0) {
      throw std::runtime_error(label + " plane " + std::to_string(i) + " has no active voxels");
    }
  }
}

void check_mirror_symmetry(const std::vector<std::array<int, 3>> &voxels, int n) {
  std::unordered_set<std::size_t> active;
  active.reserve(voxels.size() * 2);
  for (const auto &voxel : voxels) {
    active.insert(key(voxel[0], voxel[1], voxel[2], n));
  }

  for (const auto &voxel : voxels) {
    const int mx = n - 1 - voxel[0];
    const int my = n - 1 - voxel[1];
    const int mz = n - 1 - voxel[2];
    if (active.find(key(mx, voxel[1], voxel[2], n)) == active.end()) {
      throw std::runtime_error("missing x mirror for (" + std::to_string(voxel[0]) + "," +
                               std::to_string(voxel[1]) + "," +
                               std::to_string(voxel[2]) + ")");
    }
    if (active.find(key(voxel[0], my, voxel[2], n)) == active.end()) {
      throw std::runtime_error("missing y mirror for (" + std::to_string(voxel[0]) + "," +
                               std::to_string(voxel[1]) + "," +
                               std::to_string(voxel[2]) + ")");
    }
    if (active.find(key(voxel[0], voxel[1], mz, n)) == active.end()) {
      throw std::runtime_error("missing z mirror for (" + std::to_string(voxel[0]) + "," +
                               std::to_string(voxel[1]) + "," +
                               std::to_string(voxel[2]) + ")");
    }
  }
}

void check_file(const std::filesystem::path &path, int n) {
  const std::string xml = read_file(path);
  const auto ix = read_int_array(xml, "ix");
  const auto iy = read_int_array(xml, "iy");
  const auto iz = read_int_array(xml, "iz");
  if (ix.size() != iy.size() || ix.size() != iz.size()) {
    throw std::runtime_error("ix/iy/iz arrays have inconsistent lengths");
  }
  const std::size_t expected = expected_sphere_voxels(n);
  if (ix.size() != expected) {
    throw std::runtime_error("expected " + std::to_string(expected) + " active voxels, found " +
                             std::to_string(ix.size()));
  }

  std::vector<std::array<int, 3>> voxels;
  voxels.reserve(ix.size());
  for (std::size_t i = 0; i < ix.size(); ++i) {
    const std::array<int, 3> voxel{{ix[i], iy[i], iz[i]}};
    for (int axis = 0; axis < 3; ++axis) {
      if (voxel[axis] < 0 || voxel[axis] >= n) {
        throw std::runtime_error("voxel index outside expected 0.." + std::to_string(n - 1));
      }
    }
    voxels.push_back(voxel);
  }

  check_axis_planes(voxels, n, 0, "x");
  check_axis_planes(voxels, n, 1, "y");
  check_axis_planes(voxels, n, 2, "z");
  check_mirror_symmetry(voxels, n);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: sphere-voxel-lattice-check <voxels.vtu> <resolution>\n";
    return 2;
  }

  try {
    const int resolution = std::atoi(argv[2]);
    if (resolution <= 0) {
      throw std::runtime_error("resolution must be positive");
    }
    check_file(argv[1], resolution);
  } catch (const std::exception &error) {
    std::cerr << "sphere voxel lattice check failed: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
