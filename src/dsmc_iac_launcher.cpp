#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

#ifndef IAC_DSMC_EXECUTABLE_PATH
#define IAC_DSMC_EXECUTABLE_PATH ""
#endif

namespace {

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string strip_leading_ansi(std::string line) {
  while (starts_with(line, "\033[")) {
    const std::size_t marker = line.find('m');
    if (marker == std::string::npos) {
      break;
    }
    line.erase(0, marker + 1);
  }
  return line;
}

bool is_compact_line(const std::string &line) {
  const std::string plain = strip_leading_ansi(line);
  return starts_with(plain, "[IAC]") || starts_with(plain, "[SPA]");
}

void print_usage_error(const std::string &message) {
  std::cerr << "dsmc-iac: " << message << '\n';
}

int env_int(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  char *end = nullptr;
  const long parsed = std::strtol(value, &end, 10);
  if (end == value || parsed <= 0) {
    return 0;
  }
  return static_cast<int>(parsed);
}

bool is_mpi_parallel_launch() {
  const char *size_vars[] = {
      "OMPI_COMM_WORLD_SIZE",
      "PMI_SIZE",
      "PMIX_SIZE",
      "MPI_LOCALNRANKS",
      "SLURM_NTASKS",
  };
  for (const char *name : size_vars) {
    if (env_int(name) > 1) {
      return true;
    }
  }
  return false;
}

std::vector<char *> make_exec_argv(const std::string &exe, const std::vector<std::string> &args) {
  std::vector<char *> exec_argv;
  exec_argv.reserve(args.size() + 2);
  exec_argv.push_back(const_cast<char *>(exe.c_str()));
  for (const std::string &arg : args) {
    exec_argv.push_back(const_cast<char *>(arg.c_str()));
  }
  exec_argv.push_back(nullptr);
  return exec_argv;
}

int run_direct(const std::string &exe, const std::vector<std::string> &args) {
  std::vector<char *> exec_argv = make_exec_argv(exe, args);
  execv(exe.c_str(), exec_argv.data());
  std::cerr << "dsmc-iac: failed to execute " << exe << ": " << std::strerror(errno) << '\n';
  return 127;
}

void handle_line(const std::string &line, std::deque<std::string> &tail) {
  constexpr std::size_t max_tail_lines = 700;
  if (tail.size() >= max_tail_lines) {
    tail.pop_front();
  }
  tail.push_back(line);
  if (is_compact_line(line)) {
    std::cout << line;
    std::cout.flush();
  }
}

int run_quiet(const std::string &exe, const std::vector<std::string> &args) {
  int master_fd = -1;
  const pid_t child = forkpty(&master_fd, nullptr, nullptr, nullptr);
  if (child < 0) {
    std::cerr << "dsmc-iac: failed to create pseudo-terminal: " << std::strerror(errno) << '\n';
    return 127;
  }

  if (child == 0) {
    std::vector<char *> exec_argv = make_exec_argv(exe, args);
    execv(exe.c_str(), exec_argv.data());
    std::cerr << "dsmc-iac: failed to execute " << exe << ": " << std::strerror(errno) << '\n';
    _exit(127);
  }

  std::deque<std::string> output_tail;
  std::string pending;
  char buffer[4096];
  while (true) {
    const ssize_t count = read(master_fd, buffer, sizeof(buffer));
    if (count <= 0) {
      break;
    }
    pending.append(buffer, static_cast<std::size_t>(count));
    std::size_t start = 0;
    while (true) {
      const std::size_t newline = pending.find('\n', start);
      if (newline == std::string::npos) {
        pending.erase(0, start);
        break;
      }
      handle_line(pending.substr(start, newline - start + 1), output_tail);
      start = newline + 1;
    }
  }
  if (!pending.empty()) {
    handle_line(pending, output_tail);
  }
  close(master_fd);

  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    std::cerr << "dsmc-iac: failed waiting for DSMC/SPARTA: " << std::strerror(errno) << '\n';
    return 127;
  }

  int exit_code = 0;
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }

  if (exit_code != 0) {
    std::cerr << "dsmc-iac: DSMC/SPARTA exited with status " << exit_code
              << ". Captured output follows.\n";
    if (!output_tail.empty()) {
      std::cerr << "--- captured output ---\n";
      for (const std::string &line : output_tail) {
        std::cerr << line;
      }
    }
  }
  return exit_code;
}

} // namespace

int main(int argc, char **argv) {
  const std::string exe = IAC_DSMC_EXECUTABLE_PATH;
  if (exe.empty()) {
    print_usage_error("launcher was built without IAC_DSMC_EXECUTABLE_PATH");
    return 2;
  }

  std::vector<std::string> forwarded;
  bool verbose = false;
  bool has_screen = false;
  std::string color;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--iac-dsmc-verbose") {
      verbose = true;
      continue;
    }
    if (arg == "--iac-color") {
      if (i + 1 >= argc) {
        print_usage_error("--iac-color requires auto, on, or off");
        return 2;
      }
      color = argv[++i];
      if (color != "auto" && color != "on" && color != "off") {
        print_usage_error("--iac-color requires auto, on, or off");
        return 2;
      }
      continue;
    }

    forwarded.push_back(arg);
    if (arg == "-screen" || arg == "-sc") {
      has_screen = true;
      if (i + 1 >= argc) {
        print_usage_error("-screen requires an argument");
        return 2;
      }
      forwarded.emplace_back(argv[++i]);
    }
  }

  if (!color.empty()) {
    setenv("IAC_COLOR", color.c_str(), 1);
  } else if (getenv("IAC_COLOR") == nullptr) {
    setenv("IAC_COLOR", "auto", 1);
  }

  if (verbose || has_screen || is_mpi_parallel_launch()) {
    return run_direct(exe, forwarded);
  }
  return run_quiet(exe, forwarded);
}
