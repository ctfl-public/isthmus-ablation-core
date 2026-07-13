#include "isthmus_ablation/parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace iac {
namespace {

std::string line_error(int line_number, const std::string &message) {
  return "line " + std::to_string(line_number) + ": " + message;
}

std::string source_error(const std::filesystem::path &path, int line_number,
                         const std::string &message) {
  return path.string() + ":" + std::to_string(line_number) + ": " + message;
}

std::string parse_tiff_axes(const std::string &axes, int line_number) {
  if (axes.size() != 3) {
    throw InputError(line_error(line_number, "voxel create tiff axes must contain three letters"));
  }
  bool seen_x = false;
  bool seen_y = false;
  bool seen_z = false;
  for (const char axis : axes) {
    if (axis == 'x') {
      if (seen_x) {
        throw InputError(line_error(line_number, "voxel create tiff axes must not repeat x"));
      }
      seen_x = true;
    } else if (axis == 'y') {
      if (seen_y) {
        throw InputError(line_error(line_number, "voxel create tiff axes must not repeat y"));
      }
      seen_y = true;
    } else if (axis == 'z') {
      if (seen_z) {
        throw InputError(line_error(line_number, "voxel create tiff axes must not repeat z"));
      }
      seen_z = true;
    } else {
      throw InputError(line_error(line_number, "voxel create tiff axes must use only x, y, and z"));
    }
  }
  return axes;
}

std::string parse_tiff_origin_mode(const std::string &mode, int line_number) {
  if (mode == "corner" || mode == "center") {
    return mode;
  }
  throw InputError(line_error(line_number,
                              "voxel create tiff origin must be corner or center"));
}

std::string strip_comment(const std::string &line) {
  bool in_quote = false;
  std::string out;
  for (const char c : line) {
    if (c == '"') {
      in_quote = !in_quote;
      out.push_back(c);
      continue;
    }
    if (c == '#' && !in_quote) {
      break;
    }
    out.push_back(c);
  }
  return out;
}

std::vector<std::string> tokenize(const std::string &line) {
  std::vector<std::string> tokens;
  std::string token;
  bool in_quote = false;
  for (const char c : line) {
    if (c == '"') {
      in_quote = !in_quote;
      continue;
    }
    if (!in_quote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
      continue;
    }
    token.push_back(c);
  }
  if (in_quote) {
    throw InputError("unterminated quote");
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string substitute_variables(const std::string &value,
                                 const std::unordered_map<std::string, std::string> &variables,
                                 int line_number) {
  std::string out;
  for (std::size_t i = 0; i < value.size();) {
    if (value[i] == '$' && i + 1 < value.size() && value[i + 1] == '{') {
      const std::size_t close = value.find('}', i + 2);
      if (close == std::string::npos) {
        throw InputError(line_error(line_number, "unterminated variable substitution"));
      }
      const std::string name = value.substr(i + 2, close - (i + 2));
      const auto found = variables.find(name);
      if (found == variables.end()) {
        throw InputError(line_error(line_number, "unknown variable '" + name + "'"));
      }
      out += found->second;
      i = close + 1;
    } else {
      out.push_back(value[i]);
      ++i;
    }
  }
  return out;
}

void substitute_tokens(std::vector<std::string> &tokens,
                       const std::unordered_map<std::string, std::string> &variables,
                       int line_number) {
  for (auto &token : tokens) {
    token = substitute_variables(token, variables, line_number);
  }
}

void insert_subcommand(std::vector<std::string> &tokens, const std::string &command,
                       const std::string &subcommand) {
  tokens[0] = command;
  tokens.insert(tokens.begin() + 1, subcommand);
}

void normalize_canonical_command(std::vector<std::string> &tokens) {
  if (tokens.empty()) {
    return;
  }
  const std::string command = tokens[0];
  if (command == "voxel_material") {
    insert_subcommand(tokens, "voxel", "material");
  } else if (command == "voxel_create") {
    insert_subcommand(tokens, "voxel", "create");
  } else if (command == "voxel_dump") {
    insert_subcommand(tokens, "voxel", "dump");
  } else if (command == "voxel_ablate") {
    insert_subcommand(tokens, "voxel", "ablate");
  } else if (command == "voxel_ghost") {
    insert_subcommand(tokens, "voxel", "ghost");
  } else if (command == "isthmus_surface") {
    insert_subcommand(tokens, "isthmus", "surface");
  } else if (command == "surf_flux") {
    insert_subcommand(tokens, "surface", "flux");
  } else if (command == "surf_dump") {
    insert_subcommand(tokens, "surface", "dump");
  } else if (command == "iac_timestep") {
    tokens[0] = "timestep";
  } else if (command == "iac_run") {
    tokens[0] = "run";
  } else if (command == "iac_limit") {
    tokens[0] = "limit";
  } else if (command == "iac_verify") {
    tokens[0] = "verify";
  } else if (command == "iac_stats") {
    tokens[0] = "stats";
  } else if (command == "iac_stats_style") {
    tokens[0] = "stats_style";
  }
}

void reject_legacy_command(const std::string &command, int line_number) {
  if (command == "voxel") {
    throw InputError(line_error(line_number,
                                "use DSMC-style voxel_* commands, not 'voxel <subcommand>'"));
  }
  if (command == "isthmus") {
    throw InputError(line_error(line_number, "use isthmus_surface, not 'isthmus surface'"));
  }
  if (command == "surface") {
    throw InputError(line_error(line_number,
                                "use DSMC-style surf_* commands, not 'surface <subcommand>'"));
  }
  if (command == "isthmus_surf") {
    throw InputError(line_error(line_number, "use isthmus_surface, not isthmus_surf"));
  }
  if (command == "timestep") {
    throw InputError(line_error(line_number, "use iac_timestep, not timestep"));
  }
  if (command == "run") {
    throw InputError(line_error(line_number, "use iac_run for solid advancement"));
  }
  if (command == "limit") {
    throw InputError(line_error(line_number, "use iac_limit, not limit"));
  }
  if (command == "verify") {
    throw InputError(line_error(line_number, "use iac_verify, not verify"));
  }
  if (command == "stats") {
    throw InputError(line_error(line_number, "use iac_stats, not stats"));
  }
  if (command == "stats_style") {
    throw InputError(line_error(line_number, "use iac_stats_style, not stats_style"));
  }
}

void require_size(const std::vector<std::string> &tokens, std::size_t size, int line_number) {
  if (tokens.size() < size) {
    throw InputError(line_error(line_number, "not enough arguments for '" + tokens.front() + "'"));
  }
}

double parse_double(const std::string &value, int line_number) {
  try {
    std::size_t used = 0;
    const double parsed = std::stod(value, &used);
    if (used != value.size()) {
      throw InputError(line_error(line_number, "invalid numeric value '" + value + "'"));
    }
    return parsed;
  } catch (const std::invalid_argument &) {
    throw InputError(line_error(line_number, "invalid numeric value '" + value + "'"));
  } catch (const std::out_of_range &) {
    throw InputError(line_error(line_number, "numeric value out of range '" + value + "'"));
  }
}

double parse_positive_double(const std::string &value, int line_number,
                             const std::string &name) {
  const double parsed = parse_double(value, line_number);
  if (parsed <= 0.0) {
    throw InputError(line_error(line_number, name + " must be positive"));
  }
  return parsed;
}

double parse_ratio(const std::string &value, int line_number, const std::string &name) {
  if (value == "voxel") {
    return 1.0;
  }
  const std::size_t colon = value.find(':');
  if (colon == std::string::npos) {
    return parse_positive_double(value, line_number, name);
  }
  if (value.find(':', colon + 1) != std::string::npos) {
    throw InputError(line_error(line_number, "invalid " + name + " ratio '" + value + "'"));
  }
  const double numerator = parse_positive_double(value.substr(0, colon), line_number, name);
  const double denominator = parse_positive_double(value.substr(colon + 1), line_number, name);
  return numerator / denominator;
}

std::unordered_map<std::string, std::string>::const_iterator
find_first_key(const std::unordered_map<std::string, std::string> &values,
               const std::vector<std::string> &keys) {
  for (const auto &key : keys) {
    const auto found = values.find(key);
    if (found != values.end()) {
      return found;
    }
  }
  return values.end();
}

int parse_int(const std::string &value, int line_number) {
  try {
    std::size_t used = 0;
    const int parsed = std::stoi(value, &used);
    if (used != value.size()) {
      throw InputError(line_error(line_number, "invalid integer value '" + value + "'"));
    }
    return parsed;
  } catch (const std::invalid_argument &) {
    throw InputError(line_error(line_number, "invalid integer value '" + value + "'"));
  } catch (const std::out_of_range &) {
    throw InputError(line_error(line_number, "integer value out of range '" + value + "'"));
  }
}

bool parse_bool(const std::string &value, int line_number) {
  if (value == "yes" || value == "true" || value == "on" || value == "1") {
    return true;
  }
  if (value == "no" || value == "false" || value == "off" || value == "0") {
    return false;
  }
  throw InputError(line_error(line_number, "expected boolean yes/no, got '" + value + "'"));
}

std::unordered_map<std::string, std::string>
parse_pairs(const std::vector<std::string> &tokens, std::size_t first, int line_number) {
  std::unordered_map<std::string, std::string> values;
  for (std::size_t i = first; i < tokens.size();) {
    if (i + 1 >= tokens.size()) {
      throw InputError(line_error(line_number, "keyword '" + tokens[i] + "' is missing a value"));
    }
    values[tokens[i]] = tokens[i + 1];
    i += 2;
  }
  return values;
}

std::string required(const std::unordered_map<std::string, std::string> &values,
                     const std::string &key, int line_number) {
  const auto it = values.find(key);
  if (it == values.end()) {
    throw InputError(line_error(line_number, "missing keyword '" + key + "'"));
  }
  return it->second;
}

void parse_input_file_into(const std::filesystem::path &path, Config &config,
                           std::vector<std::filesystem::path> &include_stack,
                           std::unordered_map<std::string, std::string> &variables,
                           const std::unordered_map<std::string, std::string> &overrides) {
  std::ifstream input(path);
  if (!input) {
    throw InputError("could not open input file '" + path.string() + "'");
  }

  const auto canonical_path = std::filesystem::weakly_canonical(path);
  for (const auto &entry : include_stack) {
    if (entry == canonical_path) {
      throw InputError("recursive include of '" + canonical_path.string() + "'");
    }
  }
  include_stack.push_back(canonical_path);

  std::string line;
  std::string logical_line;
  int logical_line_number = 0;
  int line_number = 0;
  bool pending_continue = false;
  double pending_continue_time = 0.0;
  while (std::getline(input, line)) {
    ++line_number;
    std::string stripped = strip_comment(line);
    stripped.erase(stripped.find_last_not_of(" \t\r\n") + 1);
    bool continued = false;
    if (!stripped.empty() && stripped.back() == '&') {
      stripped.pop_back();
      stripped.erase(stripped.find_last_not_of(" \t\r\n") + 1);
      continued = true;
    }
    if (!stripped.empty()) {
      if (logical_line.empty()) {
        logical_line_number = line_number;
      } else {
        logical_line += ' ';
      }
      logical_line += stripped;
    }
    if (continued) {
      continue;
    }
    if (logical_line.empty()) {
      continue;
    }

    std::vector<std::string> tokens;
    try {
      tokens = tokenize(logical_line);
    } catch (const InputError &error) {
      throw InputError(source_error(path, logical_line_number, error.what()));
    }
    const int command_line_number = logical_line_number;
    logical_line.clear();
    if (tokens.empty()) {
      continue;
    }
    if (!(tokens.size() >= 4 && tokens[0] == "variable" && tokens[2] == "equal")) {
      substitute_tokens(tokens, variables, command_line_number);
    }
    const std::string original_command = tokens[0];
    reject_legacy_command(original_command, command_line_number);
    normalize_canonical_command(tokens);

    const auto &command = tokens[0];
    if (command == "include") {
      require_size(tokens, 2, command_line_number);
      if (tokens.size() != 2) {
        throw InputError(source_error(path, command_line_number,
                                      "include takes exactly one file path"));
      }
      auto include_path = std::filesystem::path(tokens[1]);
      if (include_path.is_relative()) {
        const auto input_relative = path.parent_path() / include_path;
        include_path = std::filesystem::exists(input_relative) ? input_relative : include_path;
      }
      parse_input_file_into(include_path, config, include_stack, variables, overrides);
    } else if (command == "units") {
      require_size(tokens, 2, line_number);
      config.units = tokens[1];
    } else if (command == "voxel") {
      require_size(tokens, 2, line_number);
      const auto &subcommand = tokens[1];
      if (subcommand == "material") {
        require_size(tokens, 5, line_number);
        Material material;
        material.name = tokens[2];
        const auto values = parse_pairs(tokens, 3, line_number);
        material.density = parse_double(required(values, "density", line_number), line_number);
        const auto id = values.find("id");
        if (id != values.end()) {
          material.id = parse_int(id->second, line_number);
        } else {
          material.id = static_cast<int>(config.materials.size()) + 1;
        }
        const auto molar_mass = values.find("molar-mass");
        if (molar_mass != values.end()) {
          material.molar_mass = parse_double(molar_mass->second, line_number);
        }
        const auto formula = values.find("formula");
        if (formula != values.end()) {
          material.formula = formula->second;
        }
        auto existing = std::find_if(config.materials.begin(), config.materials.end(),
                                     [&](const Material &entry) {
                                       return entry.name == material.name || entry.id == material.id;
                                     });
        if (existing != config.materials.end()) {
          *existing = material;
        } else {
          config.materials.push_back(material);
        }
        if (config.material.name.empty()) {
          config.material = material;
        }
      } else if (subcommand == "create") {
        require_size(tokens, 5, line_number);
        config.voxel_name = tokens[2];
        VoxelCreate create;
        create.voxel_name = tokens[2];
        const auto &kind = tokens[3];
        const auto values = parse_pairs(tokens, 4, line_number);
        if (kind == "slab") {
          create.geometry = GeometryKind::Slab;
          create.slab.nx = parse_int(required(values, "nx", line_number), line_number);
          create.slab.ny = parse_int(required(values, "ny", line_number), line_number);
          create.slab.nz = parse_int(required(values, "nz", line_number), line_number);
          create.slab.dx = parse_double(required(values, "dx", line_number), line_number);
          create.slab.material = required(values, "material", line_number);
          const auto ox = values.find("ox");
          const auto oy = values.find("oy");
          const auto oz = values.find("oz");
          if (ox != values.end()) {
            create.slab.origin[0] = parse_double(ox->second, line_number);
          }
          if (oy != values.end()) {
            create.slab.origin[1] = parse_double(oy->second, line_number);
          }
          if (oz != values.end()) {
            create.slab.origin[2] = parse_double(oz->second, line_number);
          }
          config.geometry = GeometryKind::Slab;
          config.slab = create.slab;
        } else if (kind == "sphere") {
          create.geometry = GeometryKind::Sphere;
          create.sphere.diameter =
              parse_double(required(values, "diameter", line_number), line_number);
          const auto dx = values.find("dx");
          const auto resolution = values.find("resolution");
          if ((dx == values.end()) == (resolution == values.end())) {
            throw InputError(line_error(
                line_number, "voxel create sphere requires exactly one of dx or resolution"));
          }
          if (dx != values.end()) {
            create.sphere.dx = parse_double(dx->second, line_number);
            create.sphere.resolution = 0;
          } else {
            create.sphere.resolution = parse_int(resolution->second, line_number);
            if (create.sphere.resolution <= 0) {
              throw InputError(line_error(line_number,
                                          "voxel create sphere resolution must be positive"));
            }
            create.sphere.dx =
                create.sphere.diameter / static_cast<double>(create.sphere.resolution);
          }
          create.sphere.material = required(values, "material", line_number);
          const auto cx = values.find("cx");
          const auto cy = values.find("cy");
          const auto cz = values.find("cz");
          if (cx != values.end()) {
            create.sphere.center[0] = parse_double(cx->second, line_number);
          }
          if (cy != values.end()) {
            create.sphere.center[1] = parse_double(cy->second, line_number);
          }
          if (cz != values.end()) {
            create.sphere.center[2] = parse_double(cz->second, line_number);
          }
          config.geometry = GeometryKind::Sphere;
          config.sphere = create.sphere;
        } else if (kind == "tiff") {
          create.geometry = GeometryKind::Tiff;
          create.tiff.file = required(values, "file", line_number);
          create.tiff.dx = parse_double(required(values, "dx", line_number), line_number);
          const auto material = values.find("material");
          const auto materials = values.find("materials");
          if (material == values.end() && materials == values.end()) {
            throw InputError(line_error(
                line_number, "voxel create tiff requires material <name> or materials labels"));
          }
          if (material != values.end()) {
            create.tiff.material = material->second;
          }
          if (materials != values.end()) {
            if (materials->second != "labels") {
              throw InputError(
                  line_error(line_number, "voxel create tiff materials must be labels"));
            }
            create.tiff.material_labels = true;
          }
          const auto ox = values.find("ox");
          const auto oy = values.find("oy");
          const auto oz = values.find("oz");
          if (ox != values.end()) {
            create.tiff.origin[0] = parse_double(ox->second, line_number);
          }
          if (oy != values.end()) {
            create.tiff.origin[1] = parse_double(oy->second, line_number);
          }
          if (oz != values.end()) {
            create.tiff.origin[2] = parse_double(oz->second, line_number);
          }
          const auto axes = values.find("axes");
          if (axes != values.end()) {
            create.tiff.axes = parse_tiff_axes(axes->second, line_number);
          }
          const auto origin = values.find("origin");
          const auto origin_mode = values.find("origin-mode");
          if (origin != values.end() && origin_mode != values.end()) {
            throw InputError(line_error(
                line_number, "voxel create tiff accepts either origin or origin-mode, not both"));
          }
          if (origin != values.end()) {
            create.tiff.origin_mode = parse_tiff_origin_mode(origin->second, line_number);
          }
          if (origin_mode != values.end()) {
            create.tiff.origin_mode = parse_tiff_origin_mode(origin_mode->second, line_number);
          }
          config.geometry = GeometryKind::Tiff;
          config.tiff = create.tiff;
        } else {
          throw InputError(line_error(
              line_number, "voxel create geometry must be slab, sphere, or tiff"));
        }
        config.creates.push_back(std::move(create));
      } else if (subcommand == "dump") {
        if (tokens.size() == 3 && tokens[2] == "off") {
          config.dumps.clear();
          continue;
        }
        require_size(tokens, 7, line_number);
        VoxelDump dump;
        dump.id = tokens[2];
        dump.voxels = tokens[3];
        dump.style = tokens[4];
        if (dump.style != "history" && dump.style != "vtu") {
          throw InputError(line_error(line_number, "voxel dump style must be history or vtu"));
        }
        dump.every = parse_int(tokens[5], line_number);
        if (dump.every <= 0) {
          throw InputError(line_error(line_number, "voxel dump interval must be positive"));
        }
        dump.path = tokens[6];
        const auto values = parse_pairs(tokens, 7, line_number);
        const auto select = values.find("select");
        if (select != values.end()) {
          dump.select = select->second;
          if (dump.select != "all" && dump.select != "active" &&
              dump.select != "ghosted" && dump.select != "ghosts") {
            throw InputError(
                line_error(line_number,
                           "voxel dump select must be all, active, ghosted, or ghosts"));
          }
        }
        const auto scalar = values.find("scalar");
        if (scalar != values.end()) {
          dump.scalar = scalar->second;
          if (dump.scalar != "mf" && dump.scalar != "mass" && dump.scalar != "active" &&
              dump.scalar != "fixed" && dump.scalar != "id" && dump.scalar != "ix" &&
              dump.scalar != "iy" && dump.scalar != "iz" && dump.scalar != "ghost" &&
              dump.scalar != "material-id" && dump.scalar != "density") {
            throw InputError(line_error(line_number, "unknown voxel dump scalar '" +
                                                     dump.scalar + "'"));
          }
        }
        config.dumps.push_back(std::move(dump));
      } else if (subcommand == "ablate") {
        require_size(tokens, 7, line_number);
        ScriptCommand command_entry;
        command_entry.kind = CommandKind::VoxelAblate;
        command_entry.ablate.voxels = tokens[2];
        const auto values = parse_pairs(tokens, 3, line_number);
        const auto source = values.find("source");
        if (source != values.end()) {
          command_entry.ablate.source = source->second;
        }
        const auto surface = values.find("surface");
        if (surface != values.end()) {
          command_entry.ablate.surface = surface->second;
        }
        if (command_entry.ablate.source.empty() && command_entry.ablate.surface.empty()) {
          throw InputError(line_error(
              line_number, "voxel ablate requires source <name> or surface <name>"));
        }
        command_entry.ablate.policy = required(values, "policy", line_number);
        const auto face = values.find("face");
        if (face != values.end()) {
          command_entry.ablate.face = face->second;
        }
        const auto delete_it = values.find("delete");
        if (delete_it != values.end()) {
          command_entry.ablate.delete_empty = parse_bool(delete_it->second, line_number);
        }
        config.program.push_back(std::move(command_entry));
      } else if (subcommand == "ghost") {
        require_size(tokens, 8, line_number);
        VoxelGhostCommand ghost;
        ghost.voxels = tokens[2];
        const auto values = parse_pairs(tokens, 3, line_number);
        ghost.axis = required(values, "axis", line_number);
        ghost.boundary = required(values, "boundary", line_number);
        const auto side = values.find("side");
        if (side != values.end()) {
          ghost.side = side->second;
        }
        const auto layers = values.find("layers");
        if (layers != values.end()) {
          ghost.layers = parse_int(layers->second, line_number);
        }
        if (ghost.axis != "x" && ghost.axis != "y" && ghost.axis != "z") {
          throw InputError(line_error(line_number, "voxel ghost axis must be x, y, or z"));
        }
        if (ghost.side != "lo" && ghost.side != "hi" && ghost.side != "both") {
          throw InputError(line_error(line_number, "voxel ghost side must be lo, hi, or both"));
        }
        if (ghost.boundary != "infinite") {
          throw InputError(line_error(line_number,
                                      "only voxel ghost boundary infinite is supported"));
        }
        if (ghost.layers <= 0) {
          throw InputError(line_error(line_number, "voxel ghost layers must be positive"));
        }
        config.ghosts.push_back(std::move(ghost));
      } else {
        throw InputError(line_error(line_number, "unknown voxel subcommand '" + subcommand + "'"));
      }
    } else if (command == "isthmus") {
      require_size(tokens, 5, line_number);
      if (tokens[1] != "surface") {
        throw InputError(line_error(line_number, "only 'isthmus surface' is supported"));
      }
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::IsthmusSurface;
      command_entry.isthmus_surface.name = tokens[2];
      const auto values = parse_pairs(tokens, 3, line_number);
      command_entry.isthmus_surface.voxels = required(values, "voxels", line_number);
      const auto buffer = values.find("buffer");
      if (buffer != values.end()) {
        command_entry.isthmus_surface.buffer = parse_int(buffer->second, line_number);
      }
      const auto resolution = values.find("resolution");
      if (resolution != values.end()) {
        command_entry.isthmus_surface.resolution =
            parse_ratio(resolution->second, line_number, "isthmus surface resolution");
      }
      const auto iso_value =
          find_first_key(values, {"iso", "isovalue", "iso_value", "iso-value"});
      if (iso_value != values.end()) {
        command_entry.isthmus_surface.iso_value = parse_double(iso_value->second, line_number);
      }
      const auto weighting = values.find("weighting");
      if (weighting != values.end()) {
        command_entry.isthmus_surface.weighting = parse_bool(weighting->second, line_number);
      }
      const auto map = values.find("map");
      if (map != values.end()) {
        command_entry.isthmus_surface.map = parse_bool(map->second, line_number);
      }
      const auto remove_sealed_pores =
          find_first_key(values, {"remove_sealed_pores", "remove-sealed-pores"});
      if (remove_sealed_pores != values.end()) {
        command_entry.isthmus_surface.remove_sealed_pores =
            parse_bool(remove_sealed_pores->second, line_number);
      }
      const auto crop = values.find("crop");
      if (crop != values.end()) {
        if (crop->second == "real") {
          command_entry.isthmus_surface.crop_real = true;
        } else if (crop->second == "no" || crop->second == "none") {
          command_entry.isthmus_surface.crop_real = false;
        } else {
          throw InputError(line_error(line_number, "isthmus surface crop must be real or no"));
        }
      }
      config.program.push_back(std::move(command_entry));
    } else if (command == "surface") {
      require_size(tokens, 2, line_number);
      if (tokens[1] == "flux") {
        require_size(tokens, 5, line_number);
        ScriptCommand command_entry;
        command_entry.kind = CommandKind::SurfaceFlux;
        command_entry.surface_flux.surface = tokens[2];
        std::size_t pairs_begin = 3;
        if (tokens[3] == "kinetic/theory") {
          command_entry.surface_flux.style = "kinetic/theory";
          pairs_begin = 4;
        }
        const auto values = parse_pairs(tokens, pairs_begin, line_number);
        if (command_entry.surface_flux.style == "source") {
          command_entry.surface_flux.source = required(values, "source", line_number);
        } else {
          command_entry.surface_flux.pressure =
              parse_double(required(values, "pressure", line_number), line_number);
          command_entry.surface_flux.temperature =
              parse_double(required(values, "temperature", line_number), line_number);
          command_entry.surface_flux.molecular_mass =
              parse_double(required(values, "molecular-mass", line_number), line_number);
          command_entry.surface_flux.solid_mass_per_hit =
              parse_double(required(values, "solid-mass-per-hit", line_number), line_number);
          const auto mole_fraction = values.find("mole-fraction");
          if (mole_fraction != values.end()) {
            command_entry.surface_flux.mole_fraction =
                parse_double(mole_fraction->second, line_number);
          }
          const auto reaction_prob = values.find("reaction-prob");
          if (reaction_prob != values.end()) {
            command_entry.surface_flux.reaction_prob =
                parse_double(reaction_prob->second, line_number);
          }
        }
        command_entry.surface_flux.select = required(values, "select", line_number);
        if (command_entry.surface_flux.select == "normal") {
          command_entry.surface_flux.direction[0] =
              parse_double(required(values, "nx", line_number), line_number);
          command_entry.surface_flux.direction[1] =
              parse_double(required(values, "ny", line_number), line_number);
          command_entry.surface_flux.direction[2] =
              parse_double(required(values, "nz", line_number), line_number);
          const auto min_cos = values.find("min-cos");
          if (min_cos != values.end()) {
            command_entry.surface_flux.min_cos = parse_double(min_cos->second, line_number);
          }
        } else if (command_entry.surface_flux.select == "voxels") {
          command_entry.surface_flux.voxels = required(values, "voxels", line_number);
        } else if (command_entry.surface_flux.select != "all") {
          throw InputError(line_error(
              line_number, "surface flux select must be all, normal, or voxels"));
        }
        config.program.push_back(std::move(command_entry));
      } else if (tokens[1] == "dump") {
        if (tokens.size() == 3 && tokens[2] == "off") {
          config.surface_dumps.clear();
          continue;
        }
        require_size(tokens, 7, line_number);
        SurfaceDump dump;
        dump.id = tokens[2];
        dump.surface = tokens[3];
        dump.style = tokens[4];
        if (dump.style != "vtp") {
          throw InputError(line_error(line_number, "surface dump style must be vtp"));
        }
        dump.every = parse_int(tokens[5], line_number);
        if (dump.every <= 0) {
          throw InputError(line_error(line_number, "surface dump interval must be positive"));
        }
        dump.path = tokens[6];
        config.surface_dumps.push_back(std::move(dump));
      } else {
        throw InputError(line_error(line_number, "unknown surface subcommand '" +
                                                 tokens[1] + "'"));
      }
    } else if (command == "source") {
      require_size(tokens, 4, line_number);
      config.source.name = tokens[1];
      if (tokens[2] != "constant") {
        throw InputError(line_error(line_number, "only constant source is supported"));
      }
      config.source.value = parse_double(tokens[3], line_number);
      const auto values = parse_pairs(tokens, 4, line_number);
      const auto units = values.find("units");
      if (units != values.end()) {
        config.source.units = units->second;
      }
    } else if (command == "timestep") {
      require_size(tokens, 2, line_number);
      if (tokens[1] == "mass/courant") {
        require_size(tokens, 5, line_number);
        if (tokens[3] != "source") {
          throw InputError(line_error(line_number,
                                      "expected 'timestep mass/courant <value> source <name>'"));
        }
        config.timestep.kind = TimestepKind::MassCourant;
        config.timestep.courant = parse_double(tokens[2], line_number);
        config.timestep.source = tokens[4];
      } else {
        config.timestep.kind = TimestepKind::Explicit;
        config.timestep.value = parse_double(tokens[1], line_number);
      }
    } else if (command == "stats") {
      require_size(tokens, 2, line_number);
      config.stats.every = parse_int(tokens[1], line_number);
      if (config.stats.every <= 0) {
        throw InputError(line_error(line_number, "stats interval must be positive"));
      }
    } else if (command == "stats_style") {
      require_size(tokens, 2, line_number);
      config.stats.columns.assign(tokens.begin() + 1, tokens.end());
    } else if (command == "variable") {
      require_size(tokens, 4, line_number);
      if (tokens[2] == "equal") {
        if (tokens.size() != 4) {
          throw InputError(line_error(line_number, "variable equal takes exactly one value"));
        }
        const auto override = overrides.find(tokens[1]);
        variables[tokens[1]] = override == overrides.end() ? tokens[3] : override->second;
      } else if (tokens[2] == "internal") {
        if (tokens.size() != 4) {
          throw InputError(line_error(line_number, "variable internal takes exactly one value"));
        }
        variables[tokens[1]] = tokens[3];
      } else if (tokens[2] == "loop") {
        substitute_tokens(tokens, variables, line_number);
        ScriptCommand command_entry;
        command_entry.kind = CommandKind::VariableLoop;
        command_entry.name = tokens[1];
        command_entry.count = parse_int(tokens[3], line_number);
        if (command_entry.count <= 0) {
          throw InputError(line_error(line_number, "variable loop count must be positive"));
        }
        config.program.push_back(std::move(command_entry));
      } else {
        throw InputError(line_error(line_number,
                                    "variable must use equal, internal, or loop"));
      }
    } else if (command == "label") {
      require_size(tokens, 2, line_number);
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::Label;
      command_entry.name = tokens[1];
      config.program.push_back(std::move(command_entry));
    } else if (command == "next") {
      require_size(tokens, 2, line_number);
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::Next;
      command_entry.name = tokens[1];
      config.program.push_back(std::move(command_entry));
    } else if (command == "jump") {
      require_size(tokens, 3, line_number);
      if (tokens[1] != "SELF") {
        throw InputError(line_error(line_number, "only 'jump SELF <label>' is supported"));
      }
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::Jump;
      command_entry.target = tokens[2];
      if (tokens.size() > 3) {
        throw InputError(line_error(line_number,
                                    "use iac_continue with if/jump for time-limited loops"));
      }
      config.program.push_back(std::move(command_entry));
    } else if (command == "limit") {
      if (tokens.size() != 3 || tokens[1] != "time") {
        throw InputError(line_error(line_number, "expected 'limit time <target>'"));
      }
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::LimitTime;
      command_entry.time_limit = parse_double(tokens[2], line_number);
      if (command_entry.time_limit <= 0.0) {
        throw InputError(line_error(line_number, "limit time target must be positive"));
      }
      config.program.push_back(std::move(command_entry));
    } else if (command == "iac_continue" || command == "continue") {
      if (tokens.size() != 5 || tokens[1] != "time" || tokens[3] != "variable") {
        throw InputError(line_error(line_number,
                                    "expected 'iac_continue time <target> variable <name>'"));
      }
      pending_continue = true;
      pending_continue_time = parse_double(tokens[2], line_number);
      if (pending_continue_time <= 0.0) {
        throw InputError(line_error(line_number, "iac_continue time target must be positive"));
      }
      variables[tokens[4]] = "1";
    } else if (command == "if") {
      if (tokens.size() != 4 || tokens[2] != "then") {
        throw InputError(line_error(line_number,
                                    "expected 'if \"${var} > 0\" then \"jump SELF <label>\"'"));
      }
      std::vector<std::string> then_tokens;
      try {
        then_tokens = tokenize(tokens[3]);
      } catch (const InputError &error) {
        throw InputError(source_error(path, line_number, error.what()));
      }
      if (then_tokens.size() != 3 || then_tokens[0] != "jump" ||
          then_tokens[1] != "SELF") {
        throw InputError(line_error(line_number,
                                    "only 'if ... then \"jump SELF <label>\"' is supported"));
      }
      if (!pending_continue) {
        throw InputError(line_error(
            line_number,
            "standalone if/jump loops require a preceding iac_continue time command"));
      }
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::Jump;
      command_entry.target = then_tokens[2];
      command_entry.use_time_limit = true;
      command_entry.time_limit = pending_continue_time;
      config.program.push_back(std::move(command_entry));
      pending_continue = false;
    } else if (command == "run") {
      require_size(tokens, 2, line_number);
      ScriptCommand command_entry;
      command_entry.kind = CommandKind::Run;
      if (tokens[1] == "duration") {
        require_size(tokens, 3, line_number);
        command_entry.run.use_duration = true;
        command_entry.run.duration = parse_double(tokens[2], line_number);
      } else {
        command_entry.run.use_duration = false;
        command_entry.run.steps = parse_int(tokens[1], line_number);
      }
      config.run = command_entry.run;
      config.program.push_back(std::move(command_entry));
    } else if (command == "verify") {
      require_size(tokens, 6, line_number);
      if (tokens[2] != "exact") {
        throw InputError(line_error(
            line_number,
            "expected 'verify <quantity> exact <expression> tolerance <value> [percent|absolute] [norm <name>]'"));
      }
      VerificationCheck check;
      check.quantity = tokens[1];
      check.expression = tokens[3];
      if (tokens[4] != "tolerance") {
        throw InputError(line_error(line_number, "verify requires tolerance <value>"));
      }
      check.tolerance = parse_double(tokens[5], line_number);
      std::size_t i = 6;
      if (i < tokens.size() && (tokens[i] == "percent" || tokens[i] == "absolute")) {
        check.tolerance_mode = tokens[i];
        ++i;
      }
      while (i < tokens.size()) {
        if (tokens[i] == "norm") {
          if (i + 1 >= tokens.size()) {
            throw InputError(line_error(line_number, "verify norm is missing a value"));
          }
          check.norm = tokens[i + 1];
          i += 2;
        } else {
          throw InputError(line_error(line_number, "unknown verify option '" + tokens[i] + "'"));
        }
      }
      config.checks.push_back(std::move(check));
    } else if (command == "convergence") {
      require_size(tokens, 10, line_number);
      if (tokens[2] != "exact") {
        throw InputError(line_error(line_number,
                                    "expected 'convergence <quantity> exact <expression> ...'"));
      }
      ConvergenceCheck convergence;
      convergence.check.quantity = tokens[1];
      convergence.check.expression = tokens[3];
      if (tokens[4] != "tolerance") {
        throw InputError(line_error(line_number, "convergence requires tolerance <value>"));
      }
      convergence.check.tolerance = parse_double(tokens[5], line_number);
      std::size_t i = 6;
      if (i < tokens.size() &&
          (tokens[i] == "percent" || tokens[i] == "absolute")) {
        convergence.check.tolerance_mode = tokens[i];
        ++i;
      }
      while (i < tokens.size()) {
        if (tokens[i] == "norm") {
          if (i + 1 >= tokens.size()) {
            throw InputError(line_error(line_number, "convergence norm is missing a value"));
          }
          convergence.check.norm = tokens[i + 1];
          i += 2;
        } else if (tokens[i] == "vary") {
          if (i + 2 >= tokens.size()) {
            throw InputError(line_error(line_number, "convergence vary requires a name and values"));
          }
          ConvergenceVariable variable;
          variable.name = tokens[i + 1];
          i += 2;
          while (i < tokens.size() && tokens[i] != "vary" && tokens[i] != "order" &&
                 tokens[i] != "monotonic") {
            variable.values.push_back(tokens[i]);
            ++i;
          }
          if (variable.values.empty()) {
            throw InputError(line_error(line_number, "convergence vary requires values"));
          }
          convergence.variables.push_back(std::move(variable));
        } else if (tokens[i] == "order") {
          if (i + 2 >= tokens.size()) {
            throw InputError(line_error(line_number, "convergence order requires min and max"));
          }
          convergence.min_order = parse_double(tokens[i + 1], line_number);
          convergence.max_order = parse_double(tokens[i + 2], line_number);
          i += 3;
        } else if (tokens[i] == "monotonic") {
          if (i + 1 >= tokens.size()) {
            throw InputError(line_error(line_number, "convergence monotonic is missing a value"));
          }
          convergence.require_monotonic = parse_bool(tokens[i + 1], line_number);
          i += 2;
        } else {
          throw InputError(line_error(line_number,
                                      "unknown convergence option '" + tokens[i] + "'"));
        }
      }
      if (convergence.variables.empty()) {
        throw InputError(line_error(line_number, "convergence requires at least one vary clause"));
      }
      const std::size_t count = convergence.variables.front().values.size();
      for (const auto &variable : convergence.variables) {
        if (variable.values.size() != count) {
          throw InputError(line_error(line_number,
                                      "all convergence vary clauses must have the same length"));
        }
      }
      config.convergence_checks.push_back(std::move(convergence));
    } else {
      throw InputError(source_error(path, line_number, "unknown command '" + command + "'"));
    }
  }
  if (!logical_line.empty()) {
    throw InputError(source_error(path, logical_line_number,
                                  "unterminated continued command"));
  }

  include_stack.pop_back();
}

} // namespace

Config parse_input_file(const std::string &path) {
  return parse_input_file(path, {});
}

Config parse_input_file(const std::string &path,
                        const std::unordered_map<std::string, std::string> &overrides) {
  Config config;
  std::vector<std::filesystem::path> include_stack;
  std::unordered_map<std::string, std::string> variables = overrides;
  parse_input_file_into(std::filesystem::path(path), config, include_stack, variables, overrides);
  return config;
}

} // namespace iac
