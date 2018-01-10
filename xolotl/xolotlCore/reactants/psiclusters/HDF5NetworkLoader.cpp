#include "HDF5NetworkLoader.h"
#include <stdio.h>
#include <limits>
#include <algorithm>
#include <vector>
#include "PSIClusterReactionNetwork.h"
#include <xolotlPerf.h>
#include <HDF5Utils.h>

using namespace xolotlCore;

std::unique_ptr<IReactionNetwork> HDF5NetworkLoader::load(const IOptions& options) const {
	// Get the dataset from the HDF5 files
	auto networkVector = xolotlCore::HDF5Utils::readNetwork(fileName);

	// Initialization
	int numHe = 0, numV = 0, numI = 0;
	double formationEnergy = 0.0, migrationEnergy = 0.0;
	double diffusionFactor = 0.0;
	std::vector<std::reference_wrapper<Reactant> > reactants;

	// Prepare the network
    std::unique_ptr<PSIClusterReactionNetwork> network (
			new PSIClusterReactionNetwork(handlerRegistry));

	// Loop on the networkVector
	for (auto lineIt = networkVector.begin(); lineIt != networkVector.end();
			lineIt++) {
		// Composition of the cluster
		numHe = (int) (*lineIt)[0];
		numV = (int) (*lineIt)[1];
		numI = (int) (*lineIt)[2];
		// Create the cluster
		auto nextCluster = createPSICluster(numHe, numV, numI, *network);

		// Energies
		formationEnergy = (*lineIt)[3];
		migrationEnergy = (*lineIt)[4];
		diffusionFactor = (*lineIt)[5];

		// Set the formation energy
		nextCluster->setFormationEnergy(formationEnergy);
		// Set the diffusion factor and migration energy
		nextCluster->setMigrationEnergy(migrationEnergy);
		nextCluster->setDiffusionFactor(diffusionFactor);

		// Check if we want dummy reactions
		if (dummyReactions) {
			// Create a dummy cluster (just a stock Reactant)
            // from the existing cluster
            // TODO Once C++11 support is widespread, use std::make_unique.
            std::unique_ptr<Reactant> dummyCluster(new Reactant(*nextCluster));

			// Keep a ref to it so we can trigger its updates after
            // we add it to the network.
			reactants.emplace_back(*dummyCluster);

			// Give the cluster to the network
			network->add(std::move(dummyCluster));

		} else {
			// Keep a ref to it so we can trigger its updates after
            // we add it to the network.
			reactants.emplace_back(*nextCluster);

			// Give the cluster to the network
			network->add(std::move(nextCluster));
		}
	}

	// Ask reactants to update now that they are in network.
    for (IReactant& currReactant : reactants) {
        currReactant.updateFromNetwork();
	}

	// Check if we want dummy reactions
	if (!dummyReactions) {
		// Apply sectional grouping
		applySectionalGrouping(*network);
	}

	// Create the reactions
	network->createReactionConnectivity();

	// Recompute Ids and network size and redefine the connectivities
	network->reinitializeNetwork();

    // Need to use move() because return type uses smart pointer to base class,
    // not derived class that we created.
    // Some C++11 compilers accept it without the move, but apparently
    // that is not correct behavior until C++14.
	return std::move(network);
}

