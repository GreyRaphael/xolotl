#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Regression

#include <fstream>
#include <iostream>

#include <boost/test/unit_test.hpp>

#include <xolotl/core/flux/AlphaZrFitFluxHandler.h>
#include <xolotl/options/Options.h>
#include <xolotl/test/CommandLine.h>
#include <xolotl/util/MPIUtils.h>

using namespace std;
using namespace xolotl;
using namespace core;
using namespace flux;

using Kokkos::ScopeGuard;
BOOST_GLOBAL_FIXTURE(ScopeGuard);

/**
 * The test suite is responsible for testing the AlphaZrFitFluxHandler.
 */
BOOST_AUTO_TEST_SUITE(AlphaZrFitFluxHandlerTester_testSuite)

BOOST_AUTO_TEST_CASE(checkComputeIncidentFlux)
{
	// Create the option to create a network
	xolotl::options::Options opts;
	// Create a good parameter file
	std::string parameterFile = "param.txt";
	std::ofstream paramFile(parameterFile);
	paramFile << "netParam=100 0 0 100 100" << std::endl;
	paramFile.close();

	// Create a fake command line to read the options
	test::CommandLine<2> cl{{"fakeXolotlAppNameForTests", parameterFile}};
	util::mpiInit(cl.argc, cl.argv);
	opts.readParams(cl.argc, cl.argv);

	std::remove(parameterFile.c_str());

	// Create an empty grid because we want 0D
	std::vector<double> grid;
	// Specify the surface position
	int surfacePos = 0;

	// Create the network
	using NetworkType = network::ZrReactionNetwork;
	NetworkType::AmountType maxV = opts.getMaxV();
	NetworkType::AmountType maxI = opts.getMaxI();
	NetworkType::AmountType maxB = opts.getMaxImpurity();
	NetworkType network({maxV, maxB, maxI}, grid.size(), opts);
	// Get its size
	const int dof = network.getDOF();

	// Create the iron flux handler
	auto testFitFlux = make_shared<AlphaZrFitFluxHandler>(opts);
	// Set the flux amplitude
	testFitFlux->setFluxAmplitude(1.0);
	// Initialize the flux handler
	testFitFlux->initializeFluxHandler(network, surfacePos, grid);

	// Create a time
	double currTime = 1.0;

	// The array of concentration
	double newConcentration[dof];

	// Initialize their values
	for (int i = 0; i < dof; i++) {
		newConcentration[i] = 0.0;
	}

	// The pointer to the grid point we want
	double* updatedConc = &newConcentration[0];
	double* updatedConcOffset = updatedConc;

	// Update the concentrations
	testFitFlux->computeIncidentFlux(
		currTime, updatedConcOffset, 0, surfacePos);

	// Check the value at some grid points
	BOOST_REQUIRE_CLOSE(newConcentration[0], 4.13357676e-07, 0.01); // I_1
	BOOST_REQUIRE_CLOSE(newConcentration[1], 3.95414805e-08, 0.01); // I_2
	BOOST_REQUIRE_CLOSE(newConcentration[19], 2.563377e-10, 0.01); // I_20
	BOOST_REQUIRE_CLOSE(newConcentration[42], 5.116626e-11, 0.01); // I_43
	BOOST_REQUIRE_CLOSE(newConcentration[200], 2.99444219e-07, 0.01); // V_1
	BOOST_REQUIRE_CLOSE(newConcentration[219], 3.13544564e-10, 0.01); // V_20
	BOOST_REQUIRE_CLOSE(newConcentration[265], 4.849896e-11, 0.01); // V_66
	BOOST_REQUIRE_CLOSE(newConcentration[100], 0.0, 0.01); // B_1
	BOOST_REQUIRE_CLOSE(newConcentration[129], 2.287762e-11, 0.01); // B_30
	BOOST_REQUIRE_CLOSE(newConcentration[165], 5.38877e-12, 0.01); // B_66

	// Finalize MPI
	MPI_Finalize();

	return;
}

BOOST_AUTO_TEST_SUITE_END()
