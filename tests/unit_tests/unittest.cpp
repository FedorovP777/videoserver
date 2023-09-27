#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <filesystem>
#include <utils.h>
#include <FormatFilename.h>
#include <CodecContext.h>
#include <FormatContext.h>
#include <EventService.h>
#include <SharedQueue.h>


namespace
{

TEST(DeleteFileUVTest, BasicAssertions)
{
    std::cout << std::filesystem::temp_directory_path() << "\n";
    std::filesystem::path p = std::filesystem::temp_directory_path();
    string name = app::uuid() + ".tmp";
    p /= name;
    std::ofstream tmp_file(p);
    tmp_file << app::uuid();

    // Close the file
    tmp_file.close();
    EXPECT_EQ(std::filesystem::exists(p), true);
    EventService::deleteFile(p);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    EXPECT_EQ(std::filesystem::exists(p), false);
}

TEST(SharedQueueTest, BasicAssertions)
{
    SharedQueue<int *> sq;
    EXPECT_EQ(0, sq.getSize());
    auto * insert_value = new int(1);
    sq.push(insert_value);
    EXPECT_EQ(1, sq.getSize());
    auto * pop_value = sq.popIfExist();
    EXPECT_EQ(0, sq.getSize());
    EXPECT_EQ(insert_value, pop_value);
}

TEST(ConfigTest, BasicAssertions)
{
    ConfigService cs;
    cs.loadConfigFile("tests/config_test.yaml");
    StreamSetting stream = cs.getStreams()[0];
    EXPECT_EQ(stream.input_stream_setting.src, "rtsp://11.11.11.11:554/test");
    EXPECT_EQ(stream.input_stream_setting.name, "name_1");
    EXPECT_EQ(stream.output_stream_settings[0].path, "file-480-%Y-%m-%d-{unixtime}.ts");
    EXPECT_EQ(stream.output_stream_settings[0].s3_profile->s3_folder, "/test/");
    EXPECT_EQ(stream.output_stream_settings[0].del_after_upload, true);

    EXPECT_EQ(stream.output_stream_settings[1].path, "/output/file-unmuxing-%Y-%m-%d-{unixtime}.ts");
    EXPECT_EQ(stream.output_stream_settings[1].del_after_upload, false);
    auto s3_profile = cs.getS3Profile("default");
    EXPECT_EQ(s3_profile->profile_name, "default");
    EXPECT_EQ(s3_profile->endpoint_url, "192.168.2.14:9000");
    EXPECT_EQ(s3_profile->access_key_id, "access_key_id");
    EXPECT_EQ(s3_profile->secret_key, "secret_key");
    EXPECT_EQ(s3_profile->bucket_name, "my-first-bucket");
    EXPECT_EQ(s3_profile->verify_ssl, false);
}
TEST(ConfigTestUUID, BasicAssertions)
{
    EXPECT_EQ(app::uuid().length(), 36);
    EXPECT_NE(app::uuid(), app::uuid());
}
TEST(CodecContextTest, BasicAssertions)
{
    auto * codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    auto cc = make_unique<CodecContext>(codec);
    EXPECT_NE(cc->cntx, nullptr);
}
TEST(FormatContextTest, BasicAssertions)
{
    auto * fc = new FormatContext();
    EXPECT_NE(fc->cntx, nullptr);
    delete fc;
}
TEST(FormatFilenameTest, BasicAssertions)
{
    FormatFilename ff;
    EXPECT_EQ(ff.formatting("/{camera_name}.mp4", "camera№1"), "/camera№1.mp4");
    EXPECT_EQ(ff.formatting("", ""), "");
}

}
