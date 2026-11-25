#include "../logo_locator.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
    using namespace suiteosd;
    assert(NormalizeLogoName("  Das Erste  HD  ") == "das erste hd");
    assert(NormalizeLogoName("NDR / HH") == "ndr ~ hh");
    std::vector<std::string> names;
    AppendLogoVariants(names, "ZDFneo HD");
    assert(std::find(names.begin(), names.end(), "zdfneo") != names.end());
    assert(std::find(names.begin(), names.end(), "zdf_neo") != names.end());
    const std::string dir = "suiteosd-test-logos";
    mkdir(dir.c_str(), 0700);
    const std::string path = dir + "/zdfneo.png";
    std::ofstream stream(path, std::ios::binary);
    stream << "fixture";
    stream.close();
    assert(FindChannelLogoInRoots({dir}, "ZDFneo HD") == path);
    assert(FindChannelLogoInRoots({dir}, "Unknown Channel").empty());
    assert(FindChannelLogoInRoots({dir}, "../zdfneo").empty());
    std::remove(path.c_str());
    const std::string svg = dir + "/zdfneo.svg";
    std::ofstream svgFile(svg);
    svgFile << "fixture";
    svgFile.close();
    assert(FindChannelLogoInRoots({dir}, "ZDFneo HD") == svg);
    std::remove(svg.c_str());
    rmdir(dir.c_str());
    return 0;
}
