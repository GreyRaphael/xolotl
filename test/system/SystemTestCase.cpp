#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <xolotl/interface/Interface.h>
#include <xolotl/test/SystemTestCase.h>
#include <xolotl/test/config.h>
#include <xolotl/util/Filesystem.h>

namespace xolotl
{
namespace test
{
class StdOutRedirect
{
public:
	StdOutRedirect(const std::string& outputFileName)
	{
		fflush(stdout);
		int fd1 =
			open(outputFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd1 < 0) {
			throw std::runtime_error(
				"Failed to open " + outputFileName + " for writing");
		}
		_fileDesc = dup(STDOUT_FILENO);
		if (_fileDesc < 0) {
			throw std::runtime_error("Failed to duplicate standard output");
		}
		if (dup2(fd1, STDOUT_FILENO) < 0) {
			throw std::runtime_error("Failed to duplicate " + outputFileName +
				" to standard output");
		}
		close(fd1);
	}

	~StdOutRedirect() noexcept(false)
	{
		fflush(stdout);
		if (dup2(_fileDesc, STDOUT_FILENO) < 0) {
			throw std::runtime_error("Failed to reinstate standard output");
		}
	}

private:
	int _fileDesc;
};

double
diff2Norm(
	const std::vector<double>& data, const std::vector<double>& expectedData)
{
	double diffNorm = 0.0;
	double expectNorm = 0.0;
	auto sz = data.size();
	for (std::size_t i = 0; i < sz; ++i) {
		auto diff = data[i] - expectedData[i];
		auto expect = expectedData[i];
		if (std::isnan(expectedData[i])) {
			expect = 0.0;
			if (std::isnan(data[i])) {
				diff = 0.0;
			}
		}
		if (std::isinf(expectedData[i])) {
			expect = 0.0;
			if (std::isinf(data[i])) {
				diff = 0.0;
			}
		}
		diffNorm += diff * diff;
		expectNorm += expect * expect;
	}
	diffNorm = std::sqrt(diffNorm);
	expectNorm = std::sqrt(expectNorm);
	diffNorm /= expectNorm;
	return diffNorm;
}

double
diffInfNorm(
	const std::vector<double>& data, const std::vector<double>& expectedData)
{
	double diffNorm = 0.0;
	double expectNorm = 0.0;
	auto sz = data.size();
	for (std::size_t i = 0; i < sz; ++i) {
		auto diff = std::abs(data[i] - expectedData[i]);
		auto expect = std::abs(expectedData[i]);
		if (std::isnan(expectedData[i])) {
			expect = 0.0;
			if (std::isnan(data[i])) {
				diff = 0.0;
			}
		}
		if (std::isinf(expectedData[i])) {
			expect = 0.0;
			if (std::isinf(data[i])) {
				diff = 0.0;
			}
		}
		diffNorm = std::max(diffNorm, diff);
		expectNorm = std::max(expectNorm, expect);
	}
	diffNorm /= expectNorm;
	return diffNorm;
}

double
computeDiffNorm(
	const std::vector<double>& data, const std::vector<double>& expectedData)
{
	return diff2Norm(data, expectedData);
}

std::vector<double>
readOutputFile(const std::string& fileName)
{
	std::ifstream ifs(fileName);
	if (!ifs) {
		throw std::runtime_error("Unable to open file: " + fileName);
	}

	std::string line, tmpStr;
	std::vector<double> ret;
	while (getline(ifs, line)) {
		std::stringstream(line) >> tmpStr;
		if (tmpStr[0] == '#') {
			continue;
		}

		std::stringstream ss(line);
		while (ss >> tmpStr) {
			auto tmp = atof(tmpStr.c_str());
			ret.push_back(tmp);
		}
	}

	return ret;
}

const std::string SystemTestCase::_dataDir = TO_STRING(XOLOTL_TEST_DATA_DIR);
const std::string SystemTestCase::_binDir = TO_STRING(XOLOTL_BUILD_DIR);
const std::string SystemTestCase::_defaultOutputFileName = "retentionOut.txt";

SystemTestCase::SystemTestCase(
	const std::string& caseName, const std::string& outputFileName) :
	_caseName(caseName),
	_outputFileName(outputFileName)
{
}

SystemTestCase::SystemTestCase(const std::string& caseName) :
	SystemTestCase(caseName, _defaultOutputFileName)
{
}

bool
SystemTestCase::runXolotl() const
{
	auto exec = _binDir + "/xolotl/xolotl";
	auto paramsFileName = _dataDir + "/params_" + _caseName + ".txt";
	auto consoleFileName = _binDir + "/test/system/cout_" + _caseName + ".txt";

	// Redirect console output
	StdOutRedirect redir{consoleFileName};

	char* argv[] = {exec.data(), paramsFileName.data()};
	try {
		xolotl::interface::XolotlInterface{2, argv}.solveXolotl();
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return false;
	}
	catch (...) {
		return false;
	}

	return true;
}

void
SystemTestCase::checkOutput(const std::string& outputFileName,
	const std::string& expectedOutputFileName) const
{
	auto expectedData = readOutputFile(expectedOutputFileName);
	auto data = readOutputFile(outputFileName);
	BOOST_REQUIRE(expectedData.size() == data.size());
	auto diffNorm = computeDiffNorm(data, expectedData);
	BOOST_TEST(diffNorm < _tolerance);
}

void
SystemTestCase::run() const
{
	BOOST_REQUIRE(runXolotl());

	auto argc = boost::unit_test::framework::master_test_suite().argc;
	auto argv = boost::unit_test::framework::master_test_suite().argv;
	if (argc == 2 && std::strcmp(argv[1], "--approve") == 0) {
		xolotl::fs::copy_file("./" + _outputFileName,
			_dataDir + "/output/" + _caseName + ".txt",
			xolotl::fs::copy_option::overwrite_if_exists);
	}
	else {
		checkOutput(
			"./" + _outputFileName, _dataDir + "/output/" + _caseName + ".txt");
	}
}

void
SystemTestCase::copyFile(const std::string& fileName)
{
	xolotl::fs::copy_file(_dataDir + "/" + fileName, {},
		xolotl::fs::copy_option::overwrite_if_exists);
}
} // namespace test
} // namespace xolotl