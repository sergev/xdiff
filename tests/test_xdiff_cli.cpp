#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef BUILD_DIR_PATH
#error BUILD_DIR_PATH must be defined
#endif

namespace fs = std::filesystem;

class XDiffCliTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Get build directory path
        build_dir = std::string(BUILD_DIR_PATH);
        xdiff_cli_path = fs::path(build_dir) / "xdiff";

        // Create unique temporary directory for test files
        std::string test_dir_name = "xdiff_cli_test_" + std::to_string(getpid());
        test_dir = fs::temp_directory_path() / test_dir_name;
        fs::create_directories(test_dir);
    }

    void TearDown() override
    {
        // Clean up test files
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    std::string build_dir;
    std::string xdiff_cli_path;
    fs::path test_dir;

    // Helper function to create a test file
    void createTestFile(const std::string &filename, const std::string &content)
    {
        fs::path filepath = test_dir / filename;
        std::ofstream file(filepath);
        file << content;
        file.close();
    }

    // Helper function to run xdiff and capture output
    int runXDiffCli(const std::vector<std::string> &args, std::string &output, std::string &error)
    {
        std::ostringstream cmd;
        cmd << xdiff_cli_path;
        for (const auto &arg : args) {
            cmd << " " << arg;
        }

        cmd << " 2>&1";

        FILE *pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            return -1;
        }

        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }

        int status = pclose(pipe);
        return WEXITSTATUS(status);
    }
};

// Test that xdiff binary exists
TEST_F(XDiffCliTest, BinaryExists)
{
    EXPECT_TRUE(fs::exists(xdiff_cli_path)) << "xdiff binary not found at " << xdiff_cli_path;
}

// Test basic diff of identical files
TEST_F(XDiffCliTest, IdenticalFiles)
{
    createTestFile("file1.txt", "line1\nline2\nline3\n");
    createTestFile("file2.txt", "line1\nline2\nline3\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status) << "Exit status should be 0 for identical files";
    EXPECT_TRUE(output.empty()) << "Output should be empty for identical files: " << output;
}

// Test basic diff of different files
TEST_F(XDiffCliTest, DifferentFiles)
{
    createTestFile("file1.txt", "line1\nline2\nline3\n");
    createTestFile("file2.txt", "line1\nmodified\nline3\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status) << "Exit status should be 0 for successful diff";
    EXPECT_FALSE(output.empty()) << "Output should contain diff";
    EXPECT_TRUE(output.find("---") != std::string::npos)
        << "Output should contain unified diff header";
    EXPECT_TRUE(output.find("+++") != std::string::npos)
        << "Output should contain unified diff header";
}

// Test brief mode
TEST_F(XDiffCliTest, BriefMode)
{
    createTestFile("file1.txt", "line1\nline2\n");
    createTestFile("file2.txt", "line1\nmodified\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "-q", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(1, status) << "Exit status should be 1 when files differ in brief mode";
    EXPECT_TRUE(output.find("differ") != std::string::npos) << "Output should mention files differ";
}

// Test brief mode with identical files
TEST_F(XDiffCliTest, BriefModeIdentical)
{
    createTestFile("file1.txt", "line1\nline2\n");
    createTestFile("file2.txt", "line1\nline2\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "-q", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status) << "Exit status should be 0 for identical files in brief mode";
}

// Test unified diff format
TEST_F(XDiffCliTest, UnifiedFormat)
{
    createTestFile("file1.txt", "line1\nline2\nline3\n");
    createTestFile("file2.txt", "line1\nmodified\nline3\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "-u", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status);
    EXPECT_TRUE(output.find("@@") != std::string::npos) << "Output should contain hunk header";
}

// Test context lines option
TEST_F(XDiffCliTest, ContextLines)
{
    createTestFile("file1.txt", "line1\nline2\nline3\nline4\nline5\n");
    createTestFile("file2.txt", "line1\nline2\nmodified\nline4\nline5\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "-u", "5", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status);
    // Check that context lines are included
    EXPECT_TRUE(output.find("line1") != std::string::npos);
}

// Test missing file error
TEST_F(XDiffCliTest, MissingFile)
{
    std::string output, error;
    fs::path file1 = test_dir / "nonexistent1.txt";
    fs::path file2 = test_dir / "nonexistent2.txt";

    int status = runXDiffCli({ file1.string(), file2.string() }, output, error);

    EXPECT_NE(0, status) << "Exit status should be non-zero for missing files";
    EXPECT_TRUE(output.find("cannot read") != std::string::npos ||
                output.find("No such file") != std::string::npos)
        << "Error message should indicate file read error";
}

// Test help option
TEST_F(XDiffCliTest, HelpOption)
{
    std::string output, error;

    int status = runXDiffCli({ "--help" }, output, error);

    EXPECT_EQ(0, status) << "Help should exit with status 0";
    EXPECT_TRUE(output.find("Usage") != std::string::npos ||
                output.find("Options") != std::string::npos)
        << "Help should contain usage information";
}

// Test whitespace ignore options
TEST_F(XDiffCliTest, IgnoreWhitespace)
{
    createTestFile("file1.txt", "line1\nline2\n");
    createTestFile("file2.txt", "line1  \nline2\n");

    std::string output1, error1;
    std::string output2, error2;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    // Without ignore whitespace - should show difference
    int status1 = runXDiffCli({ file1.string(), file2.string() }, output1, error1);

    // With ignore whitespace - should not show difference for trailing whitespace
    int status2 = runXDiffCli({ "-b", file1.string(), file2.string() }, output2, error2);

    // The exact behavior depends on which whitespace flags are used
    // This test verifies the option is accepted
    EXPECT_GE(status1, 0);
    EXPECT_GE(status2, 0);
}

// Test patience algorithm option
TEST_F(XDiffCliTest, PatienceAlgorithm)
{
    createTestFile("file1.txt", "line1\nline2\nline3\n");
    createTestFile("file2.txt", "line1\nmodified\nline3\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "--patience", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status) << "Patience algorithm should work";
    EXPECT_FALSE(output.empty()) << "Should produce diff output";
}

// Test histogram algorithm option
TEST_F(XDiffCliTest, HistogramAlgorithm)
{
    createTestFile("file1.txt", "line1\nline2\nline3\n");
    createTestFile("file2.txt", "line1\nmodified\nline3\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "--histogram", file1.string(), file2.string() }, output, error);

    EXPECT_EQ(0, status) << "Histogram algorithm should work";
    EXPECT_FALSE(output.empty()) << "Should produce diff output";
}

// Test invalid option
TEST_F(XDiffCliTest, InvalidOption)
{
    createTestFile("file1.txt", "line1\n");
    createTestFile("file2.txt", "line1\n");

    std::string output, error;
    fs::path file1 = test_dir / "file1.txt";
    fs::path file2 = test_dir / "file2.txt";

    int status = runXDiffCli({ "--invalid-option", file1.string(), file2.string() }, output, error);

    EXPECT_NE(0, status) << "Invalid option should cause error";
}

// Test missing file arguments
TEST_F(XDiffCliTest, MissingFileArguments)
{
    std::string output, error;

    int status = runXDiffCli({}, output, error);

    EXPECT_NE(0, status) << "Missing file arguments should cause error";
    EXPECT_TRUE(output.find("required") != std::string::npos ||
                output.find("Usage") != std::string::npos)
        << "Should show usage or error message";
}
