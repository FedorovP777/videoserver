// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com
#ifndef LIBAV_TEST_CONFIGSERVICE_H
#define LIBAV_TEST_CONFIGSERVICE_H
#include <cassert>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "yaml-cpp/yaml.h"
#include "utils.h"

using namespace std;
struct NotificationEndpoint
{
    string url;
    string access_key;
    bool enable_notification;
};
struct S3Profile
{
    string profile_name;
    string endpoint_url;
    string host;
    string access_key_id;
    string secret_key;
    string bucket_name;
    string s3_folder;
    bool verify_ssl = false;
};
struct OutputStreamSetting
{
    string name;
    string path;
    int file_duration_sec = 90;
    optional<string> time_base;
    S3Profile * s3_profile = nullptr;
    bool rescale_ts_audio = false;
    bool del_after_upload = false;
    NotificationEndpoint * notification_endpoint = nullptr;
    std::unordered_map<std::string, std::string> codec_options;
    std::unordered_map<std::string, std::string> format_options;
    std::unordered_set<std::string> codec_flags;
    std::unordered_set<std::string> format_flags;
};
struct InputStreamSetting
{
    string src;
    std::unordered_map<std::string, std::string> codec_options;
    std::unordered_map<std::string, std::string> format_options;
    string name;
    bool override_audio_pts = true;
};

struct StreamSetting
{
    InputStreamSetting input_stream_setting;
    vector<OutputStreamSetting> output_stream_settings;
};
class ConfigService
{
private:
    YAML::Node config;

public:
    ConfigService() = default;

    void loadConfigFile(string path)
    {
        const filesystem::path sandbox{path};
        assert(filesystem::exists(path));
        this->config = YAML::LoadFile(path);
    }

    OutputStreamSetting getOutputStreams(YAML::Node node)
    {
        OutputStreamSetting setting;
        setting.path = node["path"].as<string>();
        setting.codec_options = getOptions(node, "codec_options");
        setting.format_options = getOptions(node, "format_options");
        setting.codec_flags = getFlags(node, "codec_flags");
        setting.format_flags = getFlags(node, "format_flags");

        if (node["time_base"].IsDefined())
        {
            setting.time_base = node["time_base"].as<string>();
        }
        if (node["rescale_ts_audio"].IsDefined())
        {
            setting.rescale_ts_audio = node["rescale_ts_audio"].as<bool>();
        }

        if (node["file_duration_sec"].IsDefined())
        {
            setting.file_duration_sec = node["file_duration_sec"].as<int>();
        }
        if (node["name"].IsDefined() && !node["name"].as<string>().empty())
        {
            setting.name = node["name"].as<string>();
        }
        else
        {
            setting.name = app::uuid();
        }
        if (node["s3_target"].IsDefined() && !node["s3_target"].as<string>().empty())
        {
            setting.s3_profile = this->getS3Profile(node["s3_target"].as<string>());

            if (node["s3_folder"].IsDefined() && !node["s3_folder"].as<string>().empty())
            {
                string s3_folder = node["s3_folder"].as<string>();
                if (s3_folder.back() != '/')
                {
                    s3_folder.append("/");
                }
                if (s3_folder[0] != '/')
                {
                    s3_folder.insert(0, "/");
                }
                setting.s3_profile->s3_folder = s3_folder;
            }
        }

        if (this->config["notification_endpoint"].IsDefined())
        {
            setting.notification_endpoint = this->getNotificationEndpoint();
        }
        setting.del_after_upload = node["del_after_upload"].as<bool>(false);

        return setting;
    }

    static unordered_map<std::string, std::string> getOptions(YAML::Node node, std::string name)
    {
        unordered_map<std::string, std::string> options = {};
        if (!node[name].IsDefined())
        {
            return options;
        }
        for (std::size_t i = 0; i < node[name].size(); i++)
        {
            const auto option_item = node[name][i];
            options[option_item["key"].as<string>()] = option_item["value"].as<string>();
        }
        return options;
    }

    static std::unordered_set<std::string> getFlags(YAML::Node node, std::string name)
    {
        std::unordered_set<std::string> flags;
        if (!node[name].IsDefined())
        {
            return flags;
        }
        for (std::size_t i = 0; i < node[name].size(); i++)
        {
            const auto option_item = node[name][i];
            flags.insert(option_item.as<string>());
        }
        return flags;
    }

    static string getInputStream(const YAML::Node & node)
    {
        stringstream ss;
        ss << node["src"];
        return ss.str();
    }

    NotificationEndpoint * getNotificationEndpoint()
    {
        if (!this->config["notification_endpoint"].IsDefined())
        {
            return nullptr;
        }

        auto node = this->config["notification_endpoint"];
        return new NotificationEndpoint{
            node["url"].as<string>(),
            node["access_key"].as<string>(),
            node["enable_notification"].as<bool>(),
        };
    }

    S3Profile * getS3Profile(string name)
    {
        if (!this->config["s3_profiles"].IsDefined())
        {
            throw runtime_error("s3_profiles not defined.");
        }
        for (std::size_t i = 0; i < this->config["s3_profiles"].size(); i++)
        {
            auto node = this->config["s3_profiles"][i];
            if (node["name"].IsDefined() && node["name"].as<string>() == name)
            {
                auto * s3profile = new S3Profile();
                s3profile->profile_name = node["name"].as<string>();
                s3profile->endpoint_url = node["endpoint"].as<string>();
                s3profile->access_key_id = node["access_key_id"].as<string>();
                s3profile->secret_key = node["secret_key"].as<string>();
                s3profile->bucket_name = node["bucket_name"].as<string>();
                s3profile->verify_ssl = node["verify_ssl"].as<bool>();
                return s3profile;
            }
        }
        throw runtime_error(fmt::format("{} profile name not defined.", name));
    }


    vector<StreamSetting> getStreams()
    {
        auto result = vector<StreamSetting>();

        for (std::size_t i = 0; i < this->config["streams"].size(); i++)
        {
            StreamSetting ss;

            auto node = this->config["streams"][i];
            if (node["src"].IsDefined())
            {
                ss.input_stream_setting.codec_options = getOptions(node, "codec_options");
                ss.input_stream_setting.format_options = getOptions(node, "format_options");
                ss.input_stream_setting.src = getInputStream(node);
                ss.input_stream_setting.name = node["name"].as<string>();
            }

            if (node["dst"].IsDefined())
            {
                for (std::size_t j = 0; j < node["dst"].size(); j++)
                {
                    ss.output_stream_settings.push_back(this->getOutputStreams(node["dst"][j]));
                }
            }
            result.push_back(ss);
        }
        return result;
    }
};

#endif // LIBAV_TEST_CONFIGSERVICE_H
