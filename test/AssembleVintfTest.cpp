/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AssembleVintfTest"

#include <android-base/logging.h>
#include <gtest/gtest.h>
#include <vintf/AssembleVintf.h>
#include <vintf/parse_string.h>

namespace android {
namespace vintf {

static bool In(const std::string& sub, const std::string& str) {
    return str.find(sub) != std::string::npos;
}
#define EXPECT_IN(sub, str) EXPECT_TRUE(In((sub), (str))) << (str);

class AssembleVintfTest : public ::testing::Test {
   public:
    virtual void SetUp() override {
        mInstance = AssembleVintf::newInstance();
        auto s = std::make_unique<std::stringstream>();
        mOutputStream = s.get();
        mInstance->setOutputStream(std::move(s));
    }
    virtual void TearDown() override { mInstance = nullptr; }

    const std::unique_ptr<AssembleVintf>& getInstance() { return mInstance; }

    std::string getOutput() { return mOutputStream->str(); }

    void resetOutput() { mOutputStream->str(""); }

    void setFakeEnvs(const std::map<std::string, std::string>& envs) {
        for (const auto& pair : envs) {
            getInstance()->setFakeEnv(pair.first, pair.second);
        }
    }

    void addInput(const std::string& name, const std::string& s) {
        getInstance()->addInputStream(name, std::make_unique<std::stringstream>(s));
    }

    std::unique_ptr<AssembleVintf> mInstance;
    // do not own this object.
    std::stringstream* mOutputStream;
};

TEST_F(AssembleVintfTest, FrameworkMatrixEmpty) {
    std::string xmlEmpty = "<compatibility-matrix version=\"1.0\" type=\"framework\" />";
    std::string kernel318 = "CONFIG_FOO=y\n";
    std::string kernel318_64 = "CONFIG_BAR=y\n";
    std::string kernel44 = "# CONFIG_FOO is not set\n";
    std::string kernel44_64 = "CONFIG_BAR=y\n";

    addInput("compatibility_matrix.empty.xml", xmlEmpty);
    setFakeEnvs({
        {"POLICYVERS", "30"},
        {"BOARD_SEPOLICY_VERS", "10000.0"},
        {"FRAMEWORK_VBMETA_VERSION", "1.0"},
    });
    getInstance()->addKernelConfigInputStream({3, 18}, "android-base.cfg",
                                              std::make_unique<std::stringstream>(kernel318));
    getInstance()->addKernelConfigInputStream({3, 18}, "android-base-arm64.cfg",
                                              std::make_unique<std::stringstream>(kernel318_64));
    getInstance()->addKernelConfigInputStream({4, 4}, "android-base.cfg",
                                              std::make_unique<std::stringstream>(kernel44));
    getInstance()->addKernelConfigInputStream({4, 4}, "android-base-arm64.cfg",
                                              std::make_unique<std::stringstream>(kernel44_64));

    EXPECT_TRUE(getInstance()->assemble());

    EXPECT_IN(
        "<compatibility-matrix version=\"1.0\" type=\"framework\">\n"
        "    <kernel version=\"3.18.0\">\n"
        "        <config>\n"
        "            <key>CONFIG_FOO</key>\n"
        "            <value type=\"tristate\">y</value>\n"
        "        </config>\n"
        "    </kernel>\n"
        "    <kernel version=\"3.18.0\">\n"
        "        <conditions>\n"
        "            <config>\n"
        "                <key>CONFIG_ARM64</key>\n"
        "                <value type=\"tristate\">y</value>\n"
        "            </config>\n"
        "        </conditions>\n"
        "        <config>\n"
        "            <key>CONFIG_BAR</key>\n"
        "            <value type=\"tristate\">y</value>\n"
        "        </config>\n"
        "    </kernel>\n"
        "    <kernel version=\"4.4.0\">\n"
        "        <config>\n"
        "            <key>CONFIG_FOO</key>\n"
        "            <value type=\"tristate\">n</value>\n"
        "        </config>\n"
        "    </kernel>\n"
        "    <kernel version=\"4.4.0\">\n"
        "        <conditions>\n"
        "            <config>\n"
        "                <key>CONFIG_ARM64</key>\n"
        "                <value type=\"tristate\">y</value>\n"
        "            </config>\n"
        "        </conditions>\n"
        "        <config>\n"
        "            <key>CONFIG_BAR</key>\n"
        "            <value type=\"tristate\">y</value>\n"
        "        </config>\n"
        "    </kernel>\n"
        "    <sepolicy>\n"
        "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
        "        <sepolicy-version>10000.0</sepolicy-version>\n"
        "    </sepolicy>\n"
        "    <avb>\n"
        "        <vbmeta-version>1.0</vbmeta-version>\n"
        "    </avb>\n"
        "</compatibility-matrix>\n",
        getOutput());
}

TEST_F(AssembleVintfTest, FrameworkMatrix) {
    std::string tail =
        "    <kernel version=\"3.18.0\">\n"
        "        <config>\n"
        "            <key>CONFIG_FOO</key>\n"
        "            <value type=\"tristate\">y</value>\n"
        "        </config>\n"
        "    </kernel>\n"
        "    <sepolicy>\n"
        "        <kernel-sepolicy-version>30</kernel-sepolicy-version>\n"
        "        <sepolicy-version>10000.0</sepolicy-version>\n"
        "    </sepolicy>\n"
        "    <avb>\n"
        "        <vbmeta-version>1.0</vbmeta-version>\n"
        "    </avb>\n"
        "</compatibility-matrix>\n";

    std::string xmlEmpty = "<compatibility-matrix version=\"1.0\" type=\"framework\">\n" + tail;

    std::string xml1 =
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"1\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>1.0</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "</compatibility-matrix>\n";

    std::string xml2 =
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"2\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>1.0-1</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "</compatibility-matrix>\n";

    std::string xml3 =
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"3\">\n"
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "</compatibility-matrix>\n";

    auto manifest = [](size_t level) {
        return "<manifest version=\"1.0\" type=\"device\" target-level=\"" + std::to_string(level) +
               "\">\n" +
               "    <hal format=\"hidl\">\n"
               "        <name>android.hardware.foo</name>\n"
               "        <version>1.1</version>\n"
               "        <transport>hwbinder</transport>\n"
               "        <interface>\n"
               "            <name>IFoo</name>\n"
               "            <instance>default</instance>\n"
               "        </interface>\n"
               "    </hal>\n"
               "    <hal format=\"hidl\">\n"
               "        <name>android.hardware.foo</name>\n"
               "        <version>2.0</version>\n"
               "        <transport>hwbinder</transport>\n"
               "        <interface>\n"
               "            <name>IFoo</name>\n"
               "            <instance>default</instance>\n"
               "        </interface>\n"
               "    </hal>\n"
               "    <sepolicy>\n"
               "        <version>10000.0</version>\n"
               "    </sepolicy>\n"
               "</manifest>\n";
    };

    addInput("compatibility_matrix.1.xml", xml1);
    addInput("compatibility_matrix.2.xml", xml2);
    addInput("compatibility_matrix.3.xml", xml3);
    addInput("compatibility_matrix.empty.xml", xmlEmpty);
    getInstance()->setFakeEnv("PRODUCT_ENFORCE_VINTF_MANIFEST", "true");

    resetOutput();
    getInstance()->setCheckInputStream(std::make_unique<std::stringstream>(manifest(1)));
    getInstance()->assemble();
    EXPECT_IN(
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"1\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>1.0-1</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n" +
            tail,
        getOutput());

    resetOutput();
    getInstance()->setCheckInputStream(std::make_unique<std::stringstream>(manifest(2)));
    getInstance()->assemble();
    EXPECT_IN(
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"2\">\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>1.0-1</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n"
        "    <hal format=\"hidl\" optional=\"true\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n" +
            tail,
        getOutput());

    resetOutput();
    getInstance()->setCheckInputStream(std::make_unique<std::stringstream>(manifest(3)));
    getInstance()->assemble();
    EXPECT_IN(
        "<compatibility-matrix version=\"1.0\" type=\"framework\" level=\"3\">\n"
        "    <hal format=\"hidl\" optional=\"false\">\n"
        "        <name>android.hardware.foo</name>\n"
        "        <version>2.0</version>\n"
        "        <interface>\n"
        "            <name>IFoo</name>\n"
        "            <instance>default</instance>\n"
        "        </interface>\n"
        "    </hal>\n" +
            tail,
        getOutput());
}

}  // namespace vintf
}  // namespace android