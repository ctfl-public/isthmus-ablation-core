#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const std::set<std::string> kBridgeFiles = {
    "iac.cpp",       "iac.h",       "iacbridge.cpp", "iacbridge.h",
    "iacgrid.cpp",   "iacgrid.h",   "isthmus.cpp",   "isthmus.h",
    "source.cpp",    "source.h",    "surface.cpp",   "surface.h",
    "voxel.cpp",     "voxel.h",
};

const std::set<std::string> kPatchedDsmcFiles = {
    "compute_react_surf.cpp",
    "compute_react_surf_mass_flux.cpp",
};

const std::set<std::string> kSkipNames = {
    "Makefile.package",
    "Makefile.package.settings",
    "style_command.h",
    "style_command.tmp",
    "style_collide.h",
    "style_collide.tmp",
    "style_compute.h",
    "style_compute.tmp",
    "style_dump.h",
    "style_dump.tmp",
    "style_fix.h",
    "style_fix.tmp",
    "style_react.h",
    "style_react.tmp",
    "style_region.h",
    "style_region.tmp",
    "style_surf_collide.h",
    "style_surf_collide.tmp",
    "style_surf_react.h",
    "style_surf_react.tmp",
};

struct Args {
  fs::path dsmc_root;
  fs::path bridge_dir;
  fs::path overlay_src;
  fs::path iac_include;
  fs::path iac_lib;
  std::vector<fs::path> isthmus_includes;
  fs::path isthmus_lib;
};

bool starts_with(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool should_skip(const std::string &name) {
  return kSkipNames.count(name) != 0 || kBridgeFiles.count(name) != 0 ||
         starts_with(name, "Obj_") || starts_with(name, "Obj_shared_") ||
         starts_with(name, "spa_");
}

void remove_existing(const fs::path &dest) {
  std::error_code ec;
  if (fs::is_directory(dest, ec) && !fs::is_symlink(dest, ec)) {
    fs::remove_all(dest, ec);
  } else {
    fs::remove(dest, ec);
  }
  if (ec) {
    throw std::runtime_error("Cannot remove " + dest.string() + ": " + ec.message());
  }
}

void symlink_or_copy(const fs::path &source, const fs::path &dest) {
  std::error_code ec;
  if (fs::exists(dest, ec) || fs::is_symlink(dest, ec)) {
    remove_existing(dest);
  }

  if (fs::is_directory(source)) {
    fs::create_directory_symlink(source, dest, ec);
  } else {
    fs::create_symlink(source, dest, ec);
  }
  if (!ec) {
    return;
  }

  ec.clear();
  if (fs::is_directory(source)) {
    fs::copy(source, dest, fs::copy_options::recursive, ec);
  } else {
    fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
  }
  if (ec) {
    throw std::runtime_error("Cannot link or copy " + source.string() + " to " +
                             dest.string() + ": " + ec.message());
  }
}

void copy_directory(const fs::path &source, const fs::path &dest) {
  std::error_code ec;
  if (fs::exists(dest, ec) || fs::is_symlink(dest, ec)) {
    remove_existing(dest);
  }
  fs::copy(source, dest, fs::copy_options::recursive, ec);
  if (ec) {
    throw std::runtime_error("Cannot copy directory " + source.string() + " to " +
                             dest.string() + ": " + ec.message());
  }
}

void clear_directory(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    fs::create_directories(path, ec);
    if (ec) {
      throw std::runtime_error("Cannot create overlay path " + path.string() + ": " +
                               ec.message());
    }
    return;
  }
  if (!fs::is_directory(path, ec)) {
    throw std::runtime_error("Overlay path exists but is not a directory: " + path.string());
  }
  for (const auto &entry : fs::directory_iterator(path)) {
    remove_existing(entry.path());
  }
}

std::string read_file(const fs::path &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("Cannot read " + path.string());
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_file(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Cannot write " + path.string());
  }
  out << text;
}

void replace_all(std::string &text, const std::string &old_text, const std::string &new_text) {
  std::size_t pos = 0;
  while ((pos = text.find(old_text, pos)) != std::string::npos) {
    text.replace(pos, old_text.size(), new_text);
    pos += new_text.size();
  }
}

void patch_collective_safe_react_surface_computes(const fs::path &overlay_src) {
  for (const auto &name : kPatchedDsmcFiles) {
    const fs::path path = overlay_src / name;
    std::string text = read_file(path);
    replace_all(text, "if (!(lines[i].mask & groupbit)) return;",
                "if (!(lines[i].mask & groupbit)) continue;");
    replace_all(text, "if (!(tris[i].mask & groupbit)) return;",
                "if (!(tris[i].mask & groupbit)) continue;");
    write_file(path, text);
  }
}

std::string join(const std::vector<std::string> &items) {
  std::string result;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0) {
      result += " ";
    }
    result += items[i];
  }
  return result;
}

void patch_machine_makefiles(const fs::path &overlay_src,
                             const std::vector<std::string> &include_flags,
                             const std::vector<std::string> &libs) {
  const fs::path make_dir = overlay_src / "MAKE";
  if (!fs::is_directory(make_dir)) {
    return;
  }
  const std::string include_text = join(include_flags);
  const std::string lib_text = join(libs);
  for (const auto &entry : fs::directory_iterator(make_dir)) {
    const std::string filename = entry.path().filename().string();
    if (!starts_with(filename, "Makefile.")) {
      continue;
    }
    std::ifstream in(entry.path());
    if (!in) {
      throw std::runtime_error("Cannot read " + entry.path().string());
    }
    std::vector<std::string> patched;
    std::string line;
    while (std::getline(in, line)) {
      const std::size_t first = line.find_first_not_of(" \t");
      const std::string stripped = first == std::string::npos ? "" : line.substr(first);
      if (starts_with(stripped, "CCFLAGS") && line.find('=') != std::string::npos &&
          !include_text.empty()) {
        patched.push_back(line + " " + include_text);
      } else if (starts_with(stripped, "LIB") && line.find('=') != std::string::npos &&
                 !lib_text.empty()) {
        patched.push_back(line + " " + lib_text);
      } else {
        patched.push_back(line);
      }
    }
    std::ofstream out(entry.path());
    if (!out) {
      throw std::runtime_error("Cannot write " + entry.path().string());
    }
    for (const auto &patched_line : patched) {
      out << patched_line << '\n';
    }
  }
}

void write_package(const fs::path &overlay_src, const fs::path &iac_include,
                   const fs::path &iac_lib,
                   const std::vector<fs::path> &isthmus_includes,
                   const fs::path &isthmus_lib) {
  std::vector<std::string> include_flags = {"-std=c++17", "-I" + iac_include.string()};
  for (const auto &path : isthmus_includes) {
    include_flags.push_back("-I" + path.string());
  }
  const std::vector<std::string> libs = {iac_lib.string(), isthmus_lib.string()};

  const std::string package =
      "# Settings generated by isthmus-ablation-core for the DSMC overlay build\n"
      "# Do not edit; rerun the IAC CMake dsmc target instead.\n"
      "\n"
      "PKG_INC =   " +
      join(include_flags) +
      "\n"
      "PKG_PATH =  \n"
      "PKG_LIB =   " +
      join(libs) +
      "\n"
      "PKG_CPP_DEPENDS = \n"
      "PKG_LINK_DEPENDS = \n"
      "\n"
      "PKG_SYSINC =  \n"
      "PKG_SYSLIB =  \n"
      "PKG_SYSPATH = \n";
  const std::string settings =
      "# Settings generated by isthmus-ablation-core for the DSMC overlay build\n"
      "# Do not edit; rerun the IAC CMake dsmc target instead.\n";

  write_file(overlay_src / "Makefile.package", package);
  write_file(overlay_src / "Makefile.package.settings", settings);
}

fs::path absolute_existing(const fs::path &path) {
  return fs::weakly_canonical(fs::absolute(path));
}

Args parse_args(int argc, char **argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    auto require_value = [&](const std::string &name) -> fs::path {
      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for " + name);
      }
      return fs::path(argv[++i]);
    };
    if (key == "--dsmc-root") {
      args.dsmc_root = require_value(key);
    } else if (key == "--bridge-dir") {
      args.bridge_dir = require_value(key);
    } else if (key == "--overlay-src") {
      args.overlay_src = require_value(key);
    } else if (key == "--iac-include") {
      args.iac_include = require_value(key);
    } else if (key == "--iac-lib") {
      args.iac_lib = require_value(key);
    } else if (key == "--isthmus-include") {
      args.isthmus_includes.push_back(require_value(key));
    } else if (key == "--isthmus-lib") {
      args.isthmus_lib = require_value(key);
    } else {
      throw std::runtime_error("Unknown argument: " + key);
    }
  }
  if (args.dsmc_root.empty() || args.bridge_dir.empty() || args.overlay_src.empty() ||
      args.iac_include.empty() || args.iac_lib.empty() || args.isthmus_lib.empty()) {
    throw std::runtime_error(
        "Usage: create-dsmc-overlay --dsmc-root <path> --bridge-dir <path> "
        "--overlay-src <path> --iac-include <path> --iac-lib <path> "
        "[--isthmus-include <path> ...] --isthmus-lib <path>");
  }
  return args;
}

void validate(const Args &args) {
  const fs::path dsmc_src = args.dsmc_root / "src";
  if (!fs::is_directory(dsmc_src)) {
    throw std::runtime_error("DSMC root does not contain src/: " + args.dsmc_root.string());
  }
  if (!fs::is_regular_file(dsmc_src / "Makefile")) {
    throw std::runtime_error("DSMC src directory does not contain Makefile: " +
                             dsmc_src.string());
  }
  if (!fs::is_directory(args.bridge_dir)) {
    throw std::runtime_error("IAC bridge directory does not exist: " +
                             args.bridge_dir.string());
  }
  if (!fs::is_regular_file(args.iac_lib)) {
    throw std::runtime_error("IAC library does not exist yet: " + args.iac_lib.string());
  }
  if (!fs::is_regular_file(args.isthmus_lib)) {
    throw std::runtime_error("ISTHMUS library does not exist: " +
                             args.isthmus_lib.string());
  }
}

}  // namespace

int main(int argc, char **argv) {
  try {
    Args args = parse_args(argc, argv);
    validate(args);

    args.dsmc_root = absolute_existing(args.dsmc_root);
    args.bridge_dir = absolute_existing(args.bridge_dir);
    args.overlay_src = fs::absolute(args.overlay_src);
    args.iac_include = absolute_existing(args.iac_include);
    args.iac_lib = absolute_existing(args.iac_lib);
    args.isthmus_lib = absolute_existing(args.isthmus_lib);
    for (auto &path : args.isthmus_includes) {
      path = absolute_existing(path);
    }

    const fs::path dsmc_src = args.dsmc_root / "src";
    clear_directory(args.overlay_src);

    std::vector<std::string> include_flags = {"-std=c++17", "-I" + args.iac_include.string()};
    for (const auto &path : args.isthmus_includes) {
      include_flags.push_back("-I" + path.string());
    }
    const std::vector<std::string> libs = {args.iac_lib.string(), args.isthmus_lib.string()};

    for (const auto &entry : fs::directory_iterator(dsmc_src)) {
      const std::string name = entry.path().filename().string();
      if (should_skip(name)) {
        continue;
      }
      const fs::path source = absolute_existing(entry.path());
      const fs::path dest = args.overlay_src / name;
      if (name == "MAKE" && fs::is_directory(entry.path())) {
        copy_directory(source, dest);
      } else if (kPatchedDsmcFiles.count(name) != 0) {
        fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
      } else {
        symlink_or_copy(source, dest);
      }
    }

    patch_collective_safe_react_surface_computes(args.overlay_src);

    for (const auto &name : kBridgeFiles) {
      const fs::path source = args.bridge_dir / name;
      if (!fs::is_regular_file(source)) {
        throw std::runtime_error("Missing bridge file: " + source.string());
      }
      symlink_or_copy(absolute_existing(source), args.overlay_src / name);
    }

    const fs::path public_include = args.iac_include / "isthmus_ablation";
    if (!fs::is_directory(public_include)) {
      throw std::runtime_error("IAC public include directory does not exist: " +
                               public_include.string());
    }
    symlink_or_copy(absolute_existing(public_include), args.overlay_src / "isthmus_ablation");

    write_package(args.overlay_src, args.iac_include, args.iac_lib, args.isthmus_includes,
                  args.isthmus_lib);
    patch_machine_makefiles(args.overlay_src, include_flags, libs);

    const std::string readme =
        "# DSMC/IAC Overlay\n"
        "\n"
        "This directory is generated by isthmus-ablation-core.\n"
        "It symlinks a clean DSMC source tree and the IAC bridge commands into a private build "
        "area.\n"
        "Delete the parent build directory to remove it.\n"
        "\n";
    write_file(args.overlay_src.parent_path() / "README.md", readme);

    std::cout << "Wrote DSMC/IAC overlay: " << args.overlay_src.string() << '\n';
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
