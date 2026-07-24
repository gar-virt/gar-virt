import app;

import std;

int main(int argc, char* argv[]) {
    try {
        try {
            ls_gitea_runner::app_main(argc, argv);
            return 0;
        } catch (const std::exception& ex) {
            std::println(std::cerr, "Error: {}", ex.what());
            throw;
        } catch (...) {
            std::println(std::cerr, "Unknown error");
            throw;
        }
    } catch (...) {
        return 1;
    }
}
