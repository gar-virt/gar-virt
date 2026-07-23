module;

export module utility;

export namespace ls_gitea_runner {

utility::Logger& global_logger() noexcept {
    static std::unique_ptr<utility::StdOutLogger> shared_instance{[] {
        auto instance{std::make_unique<utility::StdOutLogger>()};
        instance->set_level(utility::LogLevel::error);
        return instance;
    }()};
    return *shared_instance;
}

} // namespace ls_gitea_runner
