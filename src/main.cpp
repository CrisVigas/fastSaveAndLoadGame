#include <windows.h>

#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

constexpr std::string_view SAVE_ROOT{"EldenRing"};
constexpr std::string_view SAVE_FILE{"ER0000.sl2"};

constexpr std::string_view BACKUP_DIR{"backup"};
constexpr std::string_view TEMP_OLD_BACKUP_DIR{"~temp_backup.old"};

constexpr size_t STEAM_ID_LENGTH{17};

enum class Color { RESET, ERROR_COLOR, GREEN, YELLOW, BLUE };

static std::ostream& operator<<(std::ostream& os, const Color& c) {
  switch (c) {
    using enum Color;
    case RESET:
      os << "\x1B[0m";
      break;
    case ERROR_COLOR:
      os << "\x1B[1;31m";
      break;
    case GREEN:
      os << "\x1B[32m";
      break;
    case YELLOW:
      os << "\x1B[33m";
      break;
    case BLUE:
      os << "\x1B[34m";
      break;
    default:
      os << "[[Unimplemented color]]";
  }

  return os;
}

static std::filesystem::path appdata() {
  static const std::filesystem::path appdataPath{std::getenv("APPDATA")};
  return appdataPath;
}

static std::filesystem::path saveRootPath() {
  static const std::filesystem::path svRtPath{::appdata() / ::SAVE_ROOT};
  return svRtPath;
}

static std::filesystem::path steamIdPath() {
  std::filesystem::path path{"0"};

  for (const auto& entry :
       std::filesystem::directory_iterator(::saveRootPath())) {
    const std::string dirName{entry.path().filename().generic_string()};
    const size_t dirNameLength = dirName.length();

    const bool isNumeric{std::ranges::all_of(
        dirName, [](const char c) { return ('0' <= c) && (c <= '9'); })};

    if (entry.is_directory() && (dirNameLength == ::STEAM_ID_LENGTH) &&
        isNumeric) {
      path = entry.path();
      break;
    }
  }

  return path;
}

static std::string timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

  std::tm localTime;

  if (::localtime_s(&localTime, &nowTime) != 0) {
    return "Error obtaining local time";
  }

  std::ostringstream oss;
  oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

  return oss.str();
}

static bool checkOrCreateDir(const std::filesystem::path& path) {
  bool res{true};

  if (!std::filesystem::exists(path)) {
    res = std::filesystem::create_directories(path);
    std::cout << ::timestamp() << " | " << Color::BLUE << "Created directory @"
              << Color::RESET << path << '\n';
  }

  if (!res) {
    std::cerr << Color::ERROR_COLOR << "Failed to create backup directory @"
              << path << '\n';
  }

  return res;
}

static bool copyReplace(const std::filesystem::path& fromPath,
                        const std::filesystem::path& toPath,
                        const bool fromMustExist) {
  static constexpr std::string temp{"~temp"};

  const std::filesystem::path toPathTemp{toPath.string() + temp};

  bool b1{true};

  if (std::filesystem::exists(fromPath)) {
    if (std::filesystem::exists(toPath)) {
      b1 = std::filesystem::copy_file(fromPath, toPathTemp);

      if (b1) {
        b1 = std::filesystem::remove(toPath);
      } else {
        std::cerr << Color::ERROR_COLOR << "Failed to copy file @"
                  << (toPathTemp) << '\n';
      }

      std::filesystem::rename(toPathTemp, toPath);

    } else {
      b1 = std::filesystem::copy_file(fromPath, toPath);
    }
  } else if (fromMustExist) {
    std::cerr << Color::ERROR_COLOR << "Save file not found @" << fromPath
              << '\n';
    b1 = false;
  } else {
    std::cout << ::timestamp() << " | " << fromPath << Color::BLUE
              << " is clear!" << Color::RESET << '\n';
  }

  return b1;
}

static bool backup(const std::filesystem::path& savePath,
                   const std::filesystem::path& backupPath,
                   const std::filesystem::path& tempOldBackupPath) {
  const bool b1{::copyReplace(backupPath, tempOldBackupPath, false)};

  const bool b2{::copyReplace(savePath, backupPath, true)};

  if (b1 && b2) {
    std::cout << ::timestamp() << " | " << Color::GREEN << "Backed up"
              << Color::RESET << '\n';
  }

  return b1 && b2;
}

static bool restore(const std::filesystem::path& backupPath,
                    const std::filesystem::path& savePath) {
  const bool b1{::copyReplace(backupPath, savePath, true)};

  if (b1) {
    std::cout << ::timestamp() << " | " << Color::YELLOW << "Restored"
              << Color::RESET << '\n';
  }

  return b1;
}

int main() {
  const std::filesystem::path GAME_SAVE_PATH{::steamIdPath() / ::SAVE_FILE};

  const std::filesystem::path BACKUP_DIR_PATH{std::filesystem::current_path() /
                                              ::BACKUP_DIR};
  const std::filesystem::path TEMP_OLD_BACKUP_DIR_PATH{
      std::filesystem::current_path() / ::TEMP_OLD_BACKUP_DIR};

  const std::filesystem::path BACKUP_PATH{BACKUP_DIR_PATH / ::SAVE_FILE};
  const std::filesystem::path TEMP_OLD_BACKUP_PATH{TEMP_OLD_BACKUP_DIR_PATH /
                                                   ::SAVE_FILE};

  if ((!::checkOrCreateDir(BACKUP_DIR_PATH)) ||
      (!::checkOrCreateDir(TEMP_OLD_BACKUP_DIR_PATH))) {
    return EXIT_FAILURE;
  }

  ::MSG msg{0};

  if (!(static_cast<bool>(::RegisterHotKey(nullptr, 0, MOD_NOREPEAT, VK_F1)) &&
        static_cast<bool>(::RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_F5)) &&
        static_cast<bool>(::RegisterHotKey(nullptr, 2, MOD_CONTROL, 'Q')))) {
    std::cerr << Color::ERROR_COLOR << "Failed to register hotkeys!\n";
    return EXIT_FAILURE;
  }

  std::cout << "\t >> Press CTRL + Q to quit. <<\n";

  while (GetMessage(&msg, nullptr, 0, 0) != 0) {
    if (msg.message == WM_HOTKEY) {
      switch (msg.wParam) {
        case 0:
          ::backup(GAME_SAVE_PATH, BACKUP_PATH, TEMP_OLD_BACKUP_PATH);
          break;
        case 1:
          ::restore(BACKUP_PATH, GAME_SAVE_PATH);
          break;
        case 2:
          ::exit(EXIT_SUCCESS);
          break;
        default:
          break;
      }
    }
  }

  return EXIT_SUCCESS;
}