#ifndef FEINTERSTITIALCLUSTER_H
#define FEINTERSTITIALCLUSTER_H

// Includes
#include <sstream>
#include "FeCluster.h"
#include <Constants.h>

namespace xolotlCore {

/**
 * This class represents a cluster composed entirely of interstitial defects.
 */
class FeInterstitialCluster: public FeCluster {

	static std::string buildName(IReactant::SizeType nI) {
		// Set the reactant name appropriately
		std::stringstream nameStream;
		nameStream << "I_" << nI;
		return nameStream.str();
	}

public:

	/**
	 * Default constructor, deleted because we require info to construct.
	 */
	FeInterstitialCluster() = delete;

	/**
	 * The constructor. All FeInterstitialClusters must be initialized with
	 * a size.
	 *
	 * @param nI The number of interstitial defect in this cluster
	 * @param registry The performance handler registry
	 */
	FeInterstitialCluster(int nI, IReactionNetwork& _network,
			std::shared_ptr<xolotlPerf::IHandlerRegistry> registry) :
			FeCluster(_network, registry, buildName(nI)) {

		// Set the size
		size = nI;
		// Update the composition map
		composition[toCompIdx(Species::I)] = size;

		// Set the typename appropriately
		type = ReactantType::I;

		// Compute the reaction radius
		constexpr auto EightPi = 8 * xolotlCore::pi;
		reactionRadius = xolotlCore::ironLatticeConstant
				* std::cbrt((3 / EightPi) * size);

		// Bounds on He and V
		heBounds = IntegerRange<IReactant::SizeType>(
				static_cast<IReactant::SizeType>(0),
				static_cast<IReactant::SizeType>(1));
		vBounds = IntegerRange<IReactant::SizeType>(
				static_cast<IReactant::SizeType>(0),
				static_cast<IReactant::SizeType>(1));

		return;
	}

	/**
	 * Copy constructor, deleted to prevent use.
	 */
	FeInterstitialCluster(const FeInterstitialCluster& other) = delete;

	/**
	 * The Destructor
	 */
	~FeInterstitialCluster() {
	}

	/**
	 * This operation returns the total change in this cluster due its
	 * own dissociation.
	 *
	 * @return The flux due to its dissociation
	 */
	Flux getEmissionFlux() const {
		// Initial declarations
		Flux flux = FeCluster::computeEmissionFlux();

		// Compute the loss to dislocation sinks
		if (size < 2) {
			// bias * k^2 * D * C
			flux.total += sinkBias * sinkStrength * diffusionCoefficient
					* concentration;
		}

		return flux;
	}

	/**
	 * This operation computes the partial derivatives due to emission
	 * reactions.
	 *
	 * @param partials The vector into which the partial derivatives should be
	 * inserted. This vector should have a length equal to the size of the
	 * network.
	 */
	void getEmissionPartialDerivatives(std::vector<double> & partials) const override {
		// Initial declarations
		FeCluster::getEmissionPartialDerivatives(partials);

		// Compute the loss to dislocation sinks
		if (size < 2) {
			// bias * k^2 * D * C
			partials[id - 1] -= sinkBias * sinkStrength * diffusionCoefficient;
		}

		return;
	}

    /**
     * Compute our flux and use it to update concentrations.
     *
     * @param concs Concentrations we should update.
     */
    void updateConcs(double* concs) const override {

		// Compute our flux.
		auto flux = Reactant::computeFlux<FeInterstitialCluster>(*this);

		// Apply flux to current concentrations.
		updateConcsFromFlux(concs, flux);
	}
};
//end class FeInterstitialCluster

} /* end namespace xolotlCore */
#endif