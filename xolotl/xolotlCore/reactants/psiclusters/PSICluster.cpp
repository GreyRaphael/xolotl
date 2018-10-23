#include <algorithm>
#include <cassert>
#include <iterator>
#include "PSICluster.h"
#include <xolotlPerf.h>
#include <Constants.h>
#include <MathUtils.h>
#include "PSIClusterReactionNetwork.h"

namespace xolotlCore {

PSICluster::PSICluster(PSIClusterReactionNetwork& _network,
        std::shared_ptr<xolotlPerf::IHandlerRegistry> registry,
        const std::string& _name) : 

        Reactant(_network, registry, _name),
        indexList(_network.getPhaseSpaceList()),
        psDim(_network.getNumPhaseSpaceDims()) {
}


void PSICluster::resultFrom(ProductionReaction& reaction, int a[4], int b[4]) {

	// Add a cluster pair for the given reaction.
	reactingPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& newPair = reactingPairs.back();

	// NB: newPair's reactants are same as reaction's.
	// So use newPair only from here on.
	// TODO Any way to enforce this beyond splitting it into two functions?

	// Update the coefficients
	double firstDistance[5] = { }, secondDistance[5] = { };
	firstDistance[0] = 1.0, secondDistance[0] = 1.0;
	if (newPair.first.getType() == ReactantType::PSISuper) {
		auto const& super = static_cast<PSICluster const&>(newPair.first);
		for (int i = 1; i < psDim; i++) {
			firstDistance[i] = super.getDistance(b[indexList[i] - 1],
					indexList[i] - 1);
		}
	}
	if (newPair.second.getType() == ReactantType::PSISuper) {
		auto const& super = static_cast<PSICluster const&>(newPair.second);
		for (int i = 1; i < psDim; i++) {
			secondDistance[i] = super.getDistance(b[indexList[i] - 1],
					indexList[i] - 1);
		}
	}
	for (int j = 0; j < psDim; j++) {
		for (int i = 0; i < psDim; i++) {
			newPair.coefs[i][j] += firstDistance[i] * secondDistance[j];
		}
	}

	return;
}

void PSICluster::resultFrom(ProductionReaction& reaction,
		const std::vector<PendingProductionReactionInfo>& prInfos) {

	// Add a cluster pair for the given reaction.
	reactingPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& newPair = reactingPairs.back();

	// NB: newPair's reactants are same as reaction's.
	// So use newPair only from here on.
	// TODO Any way to enforce this beyond splitting it into two functions?

	// Update the coefficients
	std::for_each(prInfos.begin(), prInfos.end(),
			[&newPair,this](const PendingProductionReactionInfo& currPRI) {
				double firstDistance[5] = {}, secondDistance[5] = {};
				firstDistance[0] = 1.0, secondDistance[0] = 1.0;
				if (newPair.first.getType() == ReactantType::PSISuper) {
					auto const& super = static_cast<PSICluster const&>(newPair.first);
					for (int i = 1; i < psDim; i++) {
						firstDistance[i] = super.getDistance(currPRI.b[indexList[i] - 1], indexList[i] - 1);
					}
				}
				if (newPair.second.getType() == ReactantType::PSISuper) {
					auto const& super = static_cast<PSICluster const&>(newPair.second);
					for (int i = 1; i < psDim; i++) {
						secondDistance[i] = super.getDistance(currPRI.b[indexList[i] - 1], indexList[i] - 1);
					}
				}
				for (int j = 0; j < psDim; j++) {
					for (int i = 0; i < psDim; i++) {
						newPair.coefs[i][j] += firstDistance[i] * secondDistance[j];
					}
				}
			});

	return;
}

void PSICluster::resultFrom(ProductionReaction& reaction, IReactant& product) {

	// Add a cluster pair for the given reaction.
	reactingPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& newPair = reactingPairs.back();

	auto const& superR1 = static_cast<PSICluster const&>(newPair.first);
	auto const& superR2 = static_cast<PSICluster const&>(newPair.second);
	auto const& superProd = static_cast<PSICluster const&>(product);

	// Check if an interstitial cluster is involved
	int iSize = 0;
	if (superR1.getType() == ReactantType::I) {
		iSize = superR1.getSize();
	} else if (superR2.getType() == ReactantType::I) {
		iSize = superR2.getSize();
	}

	// Loop on the different type of clusters in grouping
	int productComp[4] = { }, singleComp[4] = { }, r1Lo[4] = { }, r1Hi[4] = { },
			width[4] = { };
	int nOverlap = 1;
	for (int i = 1; i < 5; i++) {
		// Check the boundaries in all the directions
		auto const& bounds = superProd.getBounds(i - 1);
		productComp[i - 1] = *(bounds.begin());

		if (newPair.first.getType() == ReactantType::PSISuper) {
			auto const& r1Bounds = superR1.getBounds(i - 1);
			r1Lo[i - 1] = *(r1Bounds.begin()), r1Hi[i - 1] = *(r1Bounds.end())
					- 1;
			auto const& r2Bounds = superR2.getBounds(i - 1);
			singleComp[i - 1] = *(r2Bounds.begin());
		}

		if (newPair.second.getType() == ReactantType::PSISuper) {
			auto const& r1Bounds = superR1.getBounds(i - 1);
			singleComp[i - 1] = *(r1Bounds.begin());
			auto const& r2Bounds = superR2.getBounds(i - 1);
			r1Lo[i - 1] = *(r2Bounds.begin()), r1Hi[i - 1] = *(r2Bounds.end())
					- 1;
		}

		// Special case for V and I
		if (i == 4)
			singleComp[i - 1] -= iSize;

		width[i - 1] = std::min(productComp[i - 1],
				r1Hi[i - 1] + singleComp[i - 1])
				- std::max(productComp[i - 1], r1Lo[i - 1] + singleComp[i - 1])
				+ 1;

		nOverlap *= width[i - 1];
	}

	// Compute the coefficients
	newPair.coefs[0][0] += (double) nOverlap;
	for (int i = 1; i < psDim; i++) {
		if (r1Hi[i - 1] != r1Lo[i - 1])
			newPair.coefs[0][i] += ((double) (nOverlap * 2)
					/ (double) ((r1Hi[i - 1] - r1Lo[i - 1]) * width[i - 1]))
					* firstOrderSum(
							std::max(productComp[i - 1] - singleComp[i - 1],
									r1Lo[i - 1]),
							std::min(productComp[i - 1] - singleComp[i - 1],
									r1Hi[i - 1]),
							(double) (r1Lo[i - 1] + r1Hi[i - 1]) / 2.0);
	}

	return;
}

void PSICluster::resultFrom(ProductionReaction& reaction, double *coef) {

	// Add a cluster pair for the given reaction.
	reactingPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& newPair = reactingPairs.back();

	// NB: newPair's reactants are same as reaction's.
	// So use newPair only from here on.
	// TODO Any way to enforce this beyond splitting it into two functions?

	// Update the coefficients
	int n = 0;
	for (int i = 0; i < psDim; i++) {
		for (int j = 0; j < psDim; j++) {
			newPair.coefs[i][j] += coef[n];
			n++;
		}
	}

	return;
}

void PSICluster::participateIn(ProductionReaction& reaction, int a[4]) {
	// Look for the other cluster
	auto& otherCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	std::vector<CombiningCluster>::reverse_iterator it;
	for (it = combiningReactants.rbegin(); it != combiningReactants.rend();
			++it) {
		if (&otherCluster == &(it->combining)) {
			break;
		}
	}
	if (it == combiningReactants.rend()) {
		// We did not already know about this combination.
		// Note that we combine with the other cluster in this reaction.
		combiningReactants.emplace_back(reaction, otherCluster);
		it = combiningReactants.rbegin();
	}

	// Update the coefficients
	(*it).coefs[0] += 1.0;
	if (otherCluster.getType() == ReactantType::PSISuper) {
		for (int i = 1; i < psDim; i++) {
			(*it).coefs[i] += otherCluster.getDistance(a[indexList[i] - 1],
					indexList[i] - 1);
		}
	}

	return;
}

void PSICluster::participateIn(ProductionReaction& reaction,
		const std::vector<PendingProductionReactionInfo>& prInfos) {
	// Look for the other cluster
	auto& otherCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	std::vector<CombiningCluster>::reverse_iterator it;
	for (it = combiningReactants.rbegin(); it != combiningReactants.rend();
			++it) {
		if (&otherCluster == &(it->combining)) {
			break;
		}
	}
	if (it == combiningReactants.rend()) {
		// We did not already know about this combination.
		// Note that we combine with the other cluster in this reaction.
		combiningReactants.emplace_back(reaction, otherCluster);
		it = combiningReactants.rbegin();
	}
	assert(it != combiningReactants.rend());
	auto& combCluster = *it;

	// Update the coefficients
	std::for_each(prInfos.begin(), prInfos.end(),
			[&otherCluster,&combCluster,this](const PendingProductionReactionInfo& currPRInfo) {
				// Update the coefficients
				combCluster.coefs[0] += 1.0;
				if (otherCluster.getType() == ReactantType::PSISuper) {
					for (int i = 1; i < psDim; i++) {
						combCluster.coefs[i] += otherCluster.getDistance(currPRInfo.b[indexList[i] - 1], indexList[i] - 1);
					}
				}
			});

	return;
}

void PSICluster::participateIn(ProductionReaction& reaction,
		IReactant& product) {
	// Look for the other cluster
	auto& otherCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	std::vector<CombiningCluster>::reverse_iterator it;
	for (it = combiningReactants.rbegin(); it != combiningReactants.rend();
			++it) {
		if (&otherCluster == &(it->combining)) {
			break;
		}
	}
	if (it == combiningReactants.rend()) {
		// We did not already know about this combination.
		// Note that we combine with the other cluster in this reaction.
		combiningReactants.emplace_back(reaction, otherCluster);
		it = combiningReactants.rbegin();
	}

	auto const& superProd = static_cast<PSICluster const&>(product);

	// Check if an interstitial cluster is involved
	int iSize = 0;
	if (getType() == ReactantType::I) {
		iSize = getSize();
	}

	// Loop on the different type of clusters in grouping
	int productLo[4] = { }, productHi[4] = { }, singleComp[4] = { }, r1Lo[4] =
			{ }, r1Hi[4] = { }, width[4] = { };
	int nOverlap = 1;
	for (int i = 1; i < 5; i++) {
		auto const& bounds = superProd.getBounds(i - 1);
		productLo[i - 1] = *(bounds.begin()), productHi[i - 1] = *(bounds.end())
				- 1;
		auto const& r1Bounds = otherCluster.getBounds(i - 1);
		r1Lo[i - 1] = *(r1Bounds.begin()), r1Hi[i - 1] = *(r1Bounds.end()) - 1;
		auto const& r2Bounds = getBounds(i - 1);
		singleComp[i - 1] = *(r2Bounds.begin());

		// Special case for V and I
		if (i == 4)
			singleComp[i - 1] -= iSize;

		width[i - 1] = std::min(productHi[i - 1],
				r1Hi[i - 1] + singleComp[i - 1])
				- std::max(productLo[i - 1], r1Lo[i - 1] + singleComp[i - 1])
				+ 1;

		nOverlap *= width[i - 1];
	}

	// Compute the coefficients
	(*it).coefs[0] += nOverlap;
	for (int i = 1; i < psDim; i++) {
		if (r1Hi[i - 1] != r1Lo[i - 1])
			(*it).coefs[i] += ((double) (nOverlap * 2)
					/ (double) ((r1Hi[i - 1] - r1Lo[i - 1]) * width[i - 1]))
					* firstOrderSum(
							std::max(productLo[i - 1] - singleComp[i - 1],
									r1Lo[i - 1]),
							std::min(productHi[i - 1] - singleComp[i - 1],
									r1Hi[i - 1]),
							(double) (r1Lo[i - 1] + r1Hi[i - 1]) / 2.0);
	}

	return;
}

void PSICluster::participateIn(ProductionReaction& reaction, double *coef) {
	// Look for the other cluster
	auto& otherCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	std::vector<CombiningCluster>::reverse_iterator it;
	for (it = combiningReactants.rbegin(); it != combiningReactants.rend();
			++it) {
		if (&otherCluster == &(it->combining)) {
			break;
		}
	}
	if (it == combiningReactants.rend()) {
		// We did not already know about this combination.
		// Note that we combine with the other cluster in this reaction.
		combiningReactants.emplace_back(reaction, otherCluster);
		it = combiningReactants.rbegin();
	}

	// Update the coefficients
	int n = 0;
	for (int i = 0; i < psDim; i++) {
		(*it).coefs[i] += coef[n];
		n++;
	}

	return;
}

void PSICluster::participateIn(DissociationReaction& reaction, int a[4],
		int b[4]) {
	// Look for the other cluster
	auto& emittedCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	auto it =
			std::find_if(dissociatingPairs.rbegin(), dissociatingPairs.rend(),
					[&reaction,&emittedCluster](const ClusterPair& currPair) {
						return (&(reaction.dissociating) == &static_cast<PSICluster&>(currPair.first)) and
						(&emittedCluster == &static_cast<PSICluster&>(currPair.second));

					});
	if (it == dissociatingPairs.rend()) {

		// We did not already know about it.
		// Add the pair of them where it is important that the
		// dissociating cluster is the first one
		dissociatingPairs.emplace_back(reaction,
				static_cast<PSICluster&>(reaction.dissociating),
				static_cast<PSICluster&>(emittedCluster));
		it = dissociatingPairs.rbegin();
	}

	// Update the coefficients
	(*it).coefs[0][0] += 1.0;
	if (reaction.dissociating.getType() == ReactantType::PSISuper) {
		auto const& super = static_cast<PSICluster&>(reaction.dissociating);
		for (int i = 1; i < psDim; i++) {
			(*it).coefs[i][0] += super.getDistance(a[indexList[i] - 1],
					indexList[i] - 1);
		}
	}

	return;
}

void PSICluster::participateIn(DissociationReaction& reaction,
		const std::vector<PendingProductionReactionInfo>& prInfos) {
	// Look for the other cluster
	auto& emittedCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	auto it =
			std::find_if(dissociatingPairs.rbegin(), dissociatingPairs.rend(),
					[&reaction,&emittedCluster](const ClusterPair& currPair) {
						return (&(reaction.dissociating) == &static_cast<PSICluster&>(currPair.first)) and
						(&emittedCluster == &static_cast<PSICluster&>(currPair.second));

					});
	if (it == dissociatingPairs.rend()) {

		// We did not already know about it.
		// Add the pair of them where it is important that the
		// dissociating cluster is the first one
		dissociatingPairs.emplace_back(reaction,
				static_cast<PSICluster&>(reaction.dissociating),
				static_cast<PSICluster&>(emittedCluster));
		it = dissociatingPairs.rbegin();
	}
	assert(it != dissociatingPairs.rend());
	auto& currPair = *it;

	// Update the coefficients
	std::for_each(prInfos.begin(), prInfos.end(),
			[&currPair,&reaction,this](const PendingProductionReactionInfo& currPRI) {
				// Update the coefficients
				currPair.coefs[0][0] += 1.0;
				if (reaction.dissociating.getType() == ReactantType::PSISuper) {
					auto const& super = static_cast<PSICluster&>(reaction.dissociating);
					for (int i = 1; i < psDim; i++) {
						currPair.coefs[i][0] += super.getDistance(currPRI.a[indexList[i] - 1], indexList[i] - 1);
					}
				}
			});

	return;
}

void PSICluster::participateIn(DissociationReaction& reaction,
		IReactant& disso) {
	// Look for the other cluster
	auto& emittedCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	auto it =
			std::find_if(dissociatingPairs.rbegin(), dissociatingPairs.rend(),
					[&reaction,&emittedCluster](const ClusterPair& currPair) {
						return (&(reaction.dissociating) == &static_cast<PSICluster&>(currPair.first)) and
						(&emittedCluster == &static_cast<PSICluster&>(currPair.second));

					});
	if (it == dissociatingPairs.rend()) {

		// We did not already know about it.
		// Add the pair of them where it is important that the
		// dissociating cluster is the first one
		dissociatingPairs.emplace_back(reaction,
				static_cast<PSICluster&>(reaction.dissociating),
				static_cast<PSICluster&>(emittedCluster));
		it = dissociatingPairs.rbegin();
	}

	auto const& superDisso = static_cast<PSICluster const&>(disso);

	// Check if an interstitial cluster is involved
	int iSize = 0;
	if (getType() == ReactantType::I) {
		iSize = getSize();
	}

	// Loop on the different type of clusters in grouping
	int dissoLo[4] = { }, dissoHi[4] = { }, singleComp[4] = { }, r1Lo[4] = { },
			r1Hi[4] = { }, width[4] = { };
	int nOverlap = 1;
	for (int i = 1; i < 5; i++) {
		auto const& bounds = superDisso.getBounds(i - 1);
		dissoLo[i - 1] = *(bounds.begin()), dissoHi[i - 1] = *(bounds.end()) - 1;
		auto const& r1Bounds = emittedCluster.getBounds(i - 1);
		r1Lo[i - 1] = *(r1Bounds.begin()), r1Hi[i - 1] = *(r1Bounds.end()) - 1;
		auto const& r2Bounds = getBounds(i - 1);
		singleComp[i - 1] = *(r2Bounds.begin());

		// Special case for V and I
		if (i == 4)
			singleComp[i - 1] -= iSize;

		width[i - 1] = std::min(dissoHi[i - 1], r1Hi[i - 1] + singleComp[i - 1])
				- std::max(dissoLo[i - 1], r1Lo[i - 1] + singleComp[i - 1]) + 1;

		nOverlap *= width[i - 1];
	}

	// Compute the coefficients
	(*it).coefs[0][0] += nOverlap;
	for (int i = 1; i < psDim; i++) {
		if (dissoHi[i - 1] != dissoLo[i - 1])
			(*it).coefs[i][0] +=
					((double) (2 * nOverlap)
							/ (double) ((dissoHi[i - 1] - dissoLo[i - 1])
									* width[i - 1]))
							* firstOrderSum(
									std::max(dissoLo[i - 1],
											r1Lo[i - 1] + singleComp[i - 1]),
									std::min(dissoHi[i - 1],
											r1Hi[i - 1] + singleComp[i - 1]),
									(double) (dissoLo[i - 1] + dissoHi[i - 1])
											/ 2.0);
	}

	return;
}

void PSICluster::participateIn(DissociationReaction& reaction, double *coef) {
	// Look for the other cluster
	auto& emittedCluster = static_cast<PSICluster&>(
			(reaction.first.getId() == getId()) ? reaction.second : reaction.first);

	// Check if the reaction was already added
	auto it =
			std::find_if(dissociatingPairs.rbegin(), dissociatingPairs.rend(),
					[&reaction,&emittedCluster](const ClusterPair& currPair) {
						return (&(reaction.dissociating) == &static_cast<PSICluster&>(currPair.first)) and
						(&emittedCluster == &static_cast<PSICluster&>(currPair.second));

					});
	if (it == dissociatingPairs.rend()) {

		// We did not already know about it.
		// Add the pair of them where it is important that the
		// dissociating cluster is the first one
		dissociatingPairs.emplace_back(reaction,
				static_cast<PSICluster&>(reaction.dissociating),
				static_cast<PSICluster&>(emittedCluster));
		it = dissociatingPairs.rbegin();
	}

	// Update the coefficients
	int n = 0;
	for (int i = 0; i < psDim; i++) {
		for (int j = 0; j < psDim; j++) {
			(*it).coefs[i][j] += coef[n];
			n++;
		}
	}

	return;
}

void PSICluster::emitFrom(DissociationReaction& reaction, int a[4]) {

	// Note that we emit from the reaction's reactants according to
	// the given reaction.
	// TODO do we need to check to see whether we already know about
	// this reaction?
	emissionPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& dissPair = emissionPairs.back();

	// Count the number of reactions
	dissPair.coefs[0][0] += 1.0;

	return;
}

void PSICluster::emitFrom(DissociationReaction& reaction,
		const std::vector<PendingProductionReactionInfo>& prInfos) {

	// Note that we emit from the reaction's reactants according to
	// the given reaction.
	// TODO do we need to check to see whether we already know about
	// this reaction?
	emissionPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& dissPair = emissionPairs.back();

	// Update the coefficients
	std::for_each(prInfos.begin(), prInfos.end(),
			[&dissPair](const PendingProductionReactionInfo& currPRI) {
				// Update the coefficients
				dissPair.coefs[0][0] += 1.0;
			});

	return;
}

void PSICluster::emitFrom(DissociationReaction& reaction, IReactant& disso) {

	// Note that we emit from the reaction's reactants according to
	// the given reaction.
	// TODO do we need to check to see whether we already know about
	// this reaction?
	emissionPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& dissPair = emissionPairs.back();

	auto const& superR1 = static_cast<PSICluster const&>(dissPair.first);
	auto const& superR2 = static_cast<PSICluster const&>(dissPair.second);
	auto const& superDisso = static_cast<PSICluster const&>(disso);

	// Check if an interstitial cluster is involved
	int iSize = 0;
	if (superR1.getType() == ReactantType::I) {
		iSize = superR1.getSize();
	} else if (superR2.getType() == ReactantType::I) {
		iSize = superR2.getSize();
	}

	// Loop on the different type of clusters in grouping
	int dissoLo[4] = { }, dissoHi[4] = { }, singleComp[4] = { }, r1Lo[4] = { },
			r1Hi[4] = { }, width[4] = { };
	int nOverlap = 1;
	for (int i = 1; i < 5; i++) {
		// Check the boundaries in all the directions
		auto const& bounds = superDisso.getBounds(i - 1);
		dissoLo[i - 1] = *(bounds.begin()), dissoHi[i - 1] = *(bounds.end()) - 1;

		if (dissPair.first.getType() == ReactantType::PSISuper) {
			auto const& r1Bounds = superR1.getBounds(i - 1);
			r1Lo[i - 1] = *(r1Bounds.begin()), r1Hi[i - 1] = *(r1Bounds.end())
					- 1;
			auto const& r2Bounds = superR2.getBounds(i - 1);
			singleComp[i - 1] = *(r2Bounds.begin());
		}

		if (dissPair.second.getType() == ReactantType::PSISuper) {
			auto const& r1Bounds = superR1.getBounds(i - 1);
			singleComp[i - 1] = *(r1Bounds.begin());
			auto const& r2Bounds = superR2.getBounds(i - 1);
			r1Lo[i - 1] = *(r2Bounds.begin()), r1Hi[i - 1] = *(r2Bounds.end())
					- 1;
		}

		// Special case for V and I
		if (i == 4)
			singleComp[i - 1] -= iSize;

		width[i - 1] = std::min(dissoHi[i - 1], r1Hi[i - 1] + singleComp[i - 1])
				- std::max(dissoLo[i - 1], r1Lo[i - 1] + singleComp[i - 1]) + 1;

		nOverlap *= width[i - 1];
	}

	// Compute the coefficients
	dissPair.coefs[0][0] += (double) nOverlap;

	return;
}

void PSICluster::emitFrom(DissociationReaction& reaction, double *coef) {

	// Note that we emit from the reaction's reactants according to
	// the given reaction.
	// TODO do we need to check to see whether we already know about
	// this reaction?
	emissionPairs.emplace_back(reaction,
			static_cast<PSICluster&>(reaction.first),
			static_cast<PSICluster&>(reaction.second));
	auto& dissPair = emissionPairs.back();

	// Count the number of reactions
	int n = 0;
	for (int i = 0; i < psDim; i++) {
		for (int j = 0; j < psDim; j++) {
			dissPair.coefs[i][j] += coef[n];
			n++;
		}
	}

	return;
}

static std::vector<int> getFullConnectivityVector(std::set<int> connectivitySet,
		int size) {
	// Create a vector of zeroes with size equal to the network size
	std::vector<int> connectivity(size);

	// Set the value of the connectivity array to one for each element that is
	// in the set.
	for (auto it = connectivitySet.begin(); it != connectivitySet.end(); ++it) {
		connectivity[*it - 1] = 1;
	}

	return connectivity;
}

std::vector<int> PSICluster::getReactionConnectivity() const {
	// Create the full vector from the set and return it
	return getFullConnectivityVector(reactionConnectivitySet, network.getDOF());
}

std::vector<int> PSICluster::getDissociationConnectivity() const {
	// Create the full vector from the set and return it
	return getFullConnectivityVector(dissociationConnectivitySet,
			network.getDOF());
}

void PSICluster::resetConnectivities() {
	// Shrink the arrays to save some space. (About 10% or so.)
	reactingPairs.shrink_to_fit();
	combiningReactants.shrink_to_fit();
	dissociatingPairs.shrink_to_fit();
	emissionPairs.shrink_to_fit();

	// Clear both sets
	reactionConnectivitySet.clear();
	dissociationConnectivitySet.clear();

	// Connect this cluster to itself since any reaction will affect it
	setReactionConnectivity(getId());
	setDissociationConnectivity(getId());

	// Loop on the effective reacting pairs
	std::for_each(reactingPairs.begin(), reactingPairs.end(),
			[this](const ClusterPair& currPair) {
				// The cluster is connecting to both clusters in the pair
				setReactionConnectivity(currPair.first.getId());
				setReactionConnectivity(currPair.second.getId());
				for (int i = 1; i < psDim; i++) {
					setReactionConnectivity(currPair.first.getMomentId(indexList[i]-1));
					setReactionConnectivity(currPair.second.getMomentId(indexList[i]-1));
				}
			});

	// Loop on the effective combining reactants
	std::for_each(combiningReactants.begin(), combiningReactants.end(),
			[this](const CombiningCluster& cc) {
				// The cluster is connecting to the combining cluster
				setReactionConnectivity(cc.combining.getId());
				for (int i = 1; i < psDim; i++) {
					setReactionConnectivity(cc.combining.getMomentId(indexList[i]-1));
				}
			});

	// Loop on the effective dissociating pairs
	std::for_each(dissociatingPairs.begin(), dissociatingPairs.end(),
			[this](const ClusterPair& currPair) {
				// The cluster is connecting to the dissociating cluster which
				// is the first one by definition
				setDissociationConnectivity(currPair.first.getId());
				for (int i = 0; i < psDim; i++) {
					setDissociationConnectivity(currPair.first.getMomentId(indexList[i]-1));
				}
			});

	// Don't loop on the effective emission pairs because
	// this cluster is not connected to them

	return;
}

void PSICluster::updateFromNetwork() {

	// Clear the flux-related arrays
	reactingPairs.clear();
	combiningReactants.clear();
	dissociatingPairs.clear();
	emissionPairs.clear();

	return;
}

void PSICluster::computeDissFlux0(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum dissociation flux over all our dissociating clusters.
    flux.flux = std::accumulate(dissociatingPairs0.begin(),
			dissociatingPairs0.end(), 0.0,
			[this,concs,xi](double running, const ClusterPair0& currPair) {

				auto const& dissCluster = currPair.first;
				auto lA = dissCluster.getConcentration(concs);

                auto sum = currPair.coeff0 * lA;

				// Calculate the Dissociation flux
				return running + (currPair.reaction.kConstant[xi] * sum);
			});
}


void PSICluster::getDissociationFlux(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum dissociation flux over all our dissociating clusters.
    flux.flux = std::accumulate(dissociatingPairs.begin(),
			dissociatingPairs.end(), 0.0,
			[this,&concs,&xi](double running, const ClusterPair& currPair) {
				auto const& dissCluster = currPair.first;
				double lA[5] = {};
				lA[0] = dissCluster.getConcentration(concs);
				for (int i = 1; i < psDim; i++) {
					lA[i] = dissCluster.getMoment(concs, indexList[i] - 1);
				}

				double sum = 0.0;
				for (int i = 0; i < psDim; i++) {
					sum += currPair.coefs[i][0] * lA[i];
				}

				// Calculate the Dissociation flux
				return running +
				(currPair.reaction.kConstant[xi] * sum);
			});
}


void PSICluster::computeEmitFlux0(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum rate constants from all emission pair reactions.
	flux.flux = 
        std::accumulate(emissionPairs0.begin(), emissionPairs0.end(), 0.0,
            [xi](double running, const ClusterPair0& currPair) {
                return running + (currPair.reaction.kConstant[xi] * currPair.coeff0);
            }) * getConcentration(concs);
}

void PSICluster::getEmissionFlux(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum rate constants from all emission pair reactions.
	flux.flux = 
			std::accumulate(emissionPairs.begin(), emissionPairs.end(), 0.0,
					[&xi](double running, const ClusterPair& currPair) {
						return running + (currPair.reaction.kConstant[xi] * currPair.coefs[0][0]);
					}) * getConcentration(concs);
}

void PSICluster::computeProdFlux0(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum production flux over all reacting pairs.
	flux.flux = std::accumulate(reactingPairs0.begin(), reactingPairs0.end(),
			0.0, [this,concs,xi](double running, const ClusterPair0& currPair) {

				// Get the two reacting clusters
			auto const& firstReactant = currPair.first;
			auto const& secondReactant = currPair.second;
			auto lA = firstReactant.getConcentration(concs);
			auto lB = secondReactant.getConcentration(concs);

            auto sum = currPair.coeff0 * lA * lB;

			// Update the flux
			return running + (currPair.reaction.kConstant[xi] * sum);
		});
}

void PSICluster::getProductionFlux(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum production flux over all reacting pairs.
	flux.flux = std::accumulate(reactingPairs.begin(), reactingPairs.end(),
			0.0, [this,&concs,&xi](double running, const ClusterPair& currPair) {

				// Get the two reacting clusters
			auto const& firstReactant = currPair.first;
			auto const& secondReactant = currPair.second;
			double lA[5] = {}, lB[5] = {};
			lA[0] = firstReactant.getConcentration(concs);
			lB[0] = secondReactant.getConcentration(concs);
			for (int i = 1; i < psDim; i++) {
				lA[i] = firstReactant.getMoment(concs, indexList[i] - 1);
				lB[i] = secondReactant.getMoment(concs, indexList[i] - 1);
			}

			double sum = 0.0;
			for (int j = 0; j < psDim; j++) {
				for (int i = 0; i < psDim; i++) {
					sum += currPair.coefs[i][j] * lA[i] * lB[j];
				}
			}
			// Update the flux
			return running + (currPair.reaction.kConstant[xi] *
					sum);
		});
}

void PSICluster::computeCombFlux0(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum combination flux over all clusters that combine with us.
	flux.flux = std::accumulate(combiningReactants0.begin(),
			combiningReactants0.end(), 0.0,
			[this,concs,xi](double running, const CombiningCluster0& cc) {

				// Get the cluster that combines with this one
				auto const& combiningCluster = cc.combining;
				auto lB = combiningCluster.getConcentration(concs);

                auto sum = cc.coeff0 * lB;

				// Calculate the combination flux
				return running + (cc.reaction.kConstant[xi] * sum);

			}) * getConcentration(concs);
}

void PSICluster::getCombinationFlux(const double* __restrict concs, int xi,
                                    Reactant::Flux& flux) const {

	// Sum combination flux over all clusters that combine with us.
	flux.flux = std::accumulate(combiningReactants.begin(),
			combiningReactants.end(), 0.0,
			[this,&concs,&xi](double running, const CombiningCluster& cc) {

				// Get the cluster that combines with this one
				auto const& combiningCluster = cc.combining;
				double lB[5] = {};
				lB[0] = combiningCluster.getConcentration(concs);
				for (int i = 1; i < psDim; i++) {
					lB[i] = combiningCluster.getMoment(concs, indexList[i] - 1);
				}

				double sum = 0.0;
				for (int i = 0; i < psDim; i++) {
					sum += cc.coefs[i] * lB[i];
				}
				// Calculate the combination flux
				return running + (cc.reaction.kConstant[xi] *
						sum);

			}) * getConcentration(concs);
}

void PSICluster::getPartialDerivatives(const double* __restrict concs, int i,
        std::vector<double> & partials) const {

	// Get the partial derivatives for each reaction type
	getProductionPartialDerivatives(concs, i, partials);
	getCombinationPartialDerivatives(concs, i, partials);
	getDissociationPartialDerivatives(partials, i);
	getEmissionPartialDerivatives(partials, i);

	return;
}

void PSICluster::getProductionPartialDerivatives(const double* __restrict concs, int xi,
        std::vector<double> & partials) const {

	// Production
	// A + B --> D, D being this cluster
	// The flux for D is
	// F(C_D) = k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_D)/dC_A = k+_(A,B)*C_B
	// dF(C_D)/dC_B = k+_(A,B)*C_A
	std::for_each(reactingPairs.begin(), reactingPairs.end(),
			[this,&concs,&partials,xi](const ClusterPair& currPair) {
				// Get the two reacting clusters
				auto const& firstReactant = currPair.first;
				auto const& secondReactant = currPair.second;
				double lA[5] = {}, lB[5] = {};
				lA[0] = firstReactant.getConcentration(concs);
				lB[0] = secondReactant.getConcentration(concs);
				for (int i = 1; i < psDim; i++) {
					lA[i] = firstReactant.getMoment(concs, indexList[i] - 1);
					lB[i] = secondReactant.getMoment(concs, indexList[i] - 1);
				}

				// Compute contribution from the first part of the reacting pair
				double value = currPair.reaction.kConstant[xi];

				double sum[5][2] = {};
				for (int j = 0; j < psDim; j++) {
					for (int i = 0; i < psDim; i++) {
						sum[j][0] += currPair.coefs[j][i] * lB[i];
						sum[j][1] += currPair.coefs[i][j] * lA[i];
					}
				}

				partials[firstReactant.getId() - 1] += value * sum[0][0];
				partials[secondReactant.getId() - 1] += value * sum[0][1];
				for (int i = 1; i < psDim; i++) {
					partials[firstReactant.getMomentId(indexList[i]-1) - 1] += value * sum[i][0];
					partials[secondReactant.getMomentId(indexList[i]-1) - 1] += value * sum[i][1];
				}
			});

	return;
}

void PSICluster::computeOneProdPartials0(const double* __restrict concs, 
        int xi,
        std::vector<double> & partials,
        const ClusterPair0& currPair) const {

	// Production
	// A + B --> D, D being this cluster
	// The flux for D is
	// F(C_D) = k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_D)/dC_A = k+_(A,B)*C_B
	// dF(C_D)/dC_B = k+_(A,B)*C_A

    // Get the two reacting clusters
    auto const& firstReactant = currPair.first;
    auto const& secondReactant = currPair.second;
    auto lA = firstReactant.getConcentration(concs);
    auto lB = secondReactant.getConcentration(concs);

    // Compute contribution from the first part of the reacting pair
    double value = currPair.reaction.kConstant[xi];

    auto sum0 = currPair.coeff0 * lB;
    auto sum1 = currPair.coeff0 * lA;

    partials[firstReactant.getId() - 1] += value * sum0;
    partials[secondReactant.getId() - 1] += value * sum1;
}

void PSICluster::computeAllProdPartials0(const double* __restrict concs, 
        int xi,
        std::vector<double> & partials) const {

    // Compute partials contribution from all production reactions.
	std::for_each(reactingPairs0.begin(), reactingPairs0.end(),
			[this,concs,&partials,xi](const ClusterPair0& currPair) {
                computeOneProdPartials0(concs, xi, partials, currPair);
			});
}

void PSICluster::getCombinationPartialDerivatives(const double* __restrict concs, int xi,
		std::vector<double> & partials) const {

	// Combination
	// A + B --> D, A being this cluster
	// The flux for A is outgoing
	// F(C_A) = - k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k+_(A,B)*C_B
	// dF(C_A)/dC_B = - k+_(A,B)*C_A
	std::for_each(combiningReactants.begin(), combiningReactants.end(),
			[this,&concs,&partials,xi](const CombiningCluster& cc) {
				auto const& cluster = cc.combining;
				double lB[5] = {};
				lB[0] = cluster.getConcentration(concs);
				for (int i = 1; i < psDim; i++) {
					lB[i] = cluster.getMoment(concs, indexList[i] - 1);
				}

				double sum = 0.0;
				for (int i = 0; i < psDim; i++) {
					sum += cc.coefs[i] * lB[i];
				}

				// Remember that the flux due to combinations is OUTGOING (-=)!
				// Compute the contribution from this cluster
				partials[getId() - 1] -= cc.reaction.kConstant[xi]
				* sum;
				// Compute the contribution from the combining cluster
				double value = cc.reaction.kConstant[xi] * getConcentration(concs);
				partials[cluster.getId() - 1] -= value * cc.coefs[0];

				for (int i = 1; i < psDim; i++) {
					partials[cluster.getMomentId(indexList[i]-1) - 1] -= value * cc.coefs[i];
				}
			});

	return;
}

void PSICluster::computeOneCombPartials0(const double* __restrict concs,
        int xi,
		std::vector<double>& partials,
        const CombiningCluster0& cc) const {

	// Combination
	// A + B --> D, A being this cluster
	// The flux for A is outgoing
	// F(C_A) = - k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k+_(A,B)*C_B
	// dF(C_A)/dC_B = - k+_(A,B)*C_A
    auto const& cluster = cc.combining;
    auto lB = cluster.getConcentration(concs);

    auto sum = cc.coeff0 * lB;

    // Remember that the flux due to combinations is OUTGOING (-=)!
    // Compute the contribution from this cluster
    partials[getId() - 1] -= cc.reaction.kConstant[xi] * sum;

    // Compute the contribution from the combining cluster
    double value = cc.reaction.kConstant[xi] * getConcentration(concs);
    partials[cluster.getId() - 1] -= value * cc.coeff0;
}

void PSICluster::computeAllCombPartials0(const double* __restrict concs,
        int xi,
		std::vector<double> & partials) const {

    // Compute partials contribution from all combining reactions.
	std::for_each(combiningReactants0.begin(), combiningReactants0.end(),
        [this,concs,&partials,xi](const CombiningCluster0& cc) {
            computeOneCombPartials0(concs, xi, partials, cc);
        });
}

void PSICluster::getDissociationPartialDerivatives(
		std::vector<double> & partials, int xi) const {

	// Dissociation
	// A --> B + D, B being this cluster
	// The flux for B is
	// F(C_B) = k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_B)/dC_A = k-_(B,D)
	std::for_each(dissociatingPairs.begin(), dissociatingPairs.end(),
			[&partials,this,&xi](const ClusterPair& currPair) {
				// Get the dissociating cluster
				auto const& cluster = currPair.first;
				double value = currPair.reaction.kConstant[xi];
				partials[cluster.getId() - 1] += value * currPair.coefs[0][0];
				for (int i = 1; i < psDim; i++) {
					partials[cluster.getMomentId(indexList[i]-1) - 1] += value * currPair.coefs[i][0];
				}
			});

	return;
}

void PSICluster::computeOneDissPartials0(int xi,
        std::vector<double>& partials,
        const ClusterPair0& currPair) const {

	// Dissociation
	// A --> B + D, B being this cluster
	// The flux for B is
	// F(C_B) = k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_B)/dC_A = k-_(B,D)

    // Get the dissociating cluster
    auto const& cluster = currPair.first;
    double value = currPair.reaction.kConstant[xi];
    partials[cluster.getId() - 1] += value * currPair.coeff0;
}

void PSICluster::computeAllDissPartials0(int xi,
        std::vector<double>& partials) const {

    // Compute partials contribution for all dissociation reactions.
	std::for_each(dissociatingPairs0.begin(), dissociatingPairs0.end(),
        [&partials,this,xi](const ClusterPair0& currPair) {
            computeOneDissPartials0(xi, partials, currPair);
        });
}

void PSICluster::getEmissionPartialDerivatives(std::vector<double> & partials,
		int xi) const {

	// Emission
	// A --> B + D, A being this cluster
	// The flux for A is
	// F(C_A) = - k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k-_(B,D)
	double outgoingFlux =
			std::accumulate(emissionPairs.begin(), emissionPairs.end(), 0.0,
					[&xi](double running, const ClusterPair& currPair) {
						return running + (currPair.reaction.kConstant[xi] * currPair.coefs[0][0]);
					});
	partials[getId() - 1] -= outgoingFlux;

	return;
}


void PSICluster::computeAllEmitPartials0(int xi,
        std::vector<double>& partials) const {

	// Emission
	// A --> B + D, A being this cluster
	// The flux for A is
	// F(C_A) = - k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k-_(B,D)
	double outgoingFlux =
			std::accumulate(emissionPairs0.begin(), emissionPairs0.end(),
                    0.0,
					[xi](double running, const ClusterPair0& currPair) {
						return running + (currPair.reaction.kConstant[xi] * currPair.coeff0);
					});
	partials[getId() - 1] -= outgoingFlux;
}

double PSICluster::getLeftSideRate(const double* __restrict concs, int i) const {

	// Sum rate constant-concentration product over combining reactants.
	double combiningRateTotal =
			std::accumulate(combiningReactants.begin(),
					combiningReactants.end(), 0.0,
					[&concs,i](double running, const CombiningCluster& cc) {
						return running +
						(cc.reaction.kConstant[i] * cc.combining.getConcentration(concs) * cc.coefs[0]);
					});

	// Sum rate constants over all emission pair reactions.
	double emissionRateTotal =
			std::accumulate(emissionPairs.begin(), emissionPairs.end(), 0.0,
					[i](double running, const ClusterPair& currPair) {
						return running + (currPair.reaction.kConstant[i] * currPair.coefs[0][0]);
					});

	return combiningRateTotal + emissionRateTotal;
}

std::vector<std::vector<double> > PSICluster::getProdVector() const {
	// Initial declarations
	std::vector<std::vector<double> > toReturn;

	// Loop on the reacting pairs
	std::for_each(reactingPairs.begin(), reactingPairs.end(),
			[&toReturn,this](const ClusterPair& currPair) {
				// Build the vector containing ids and rates
				std::vector<double> tempVec;
				tempVec.push_back(currPair.first.getId() - 1);
				tempVec.push_back(currPair.second.getId() - 1);
				for (int i = 0; i < psDim; i++) {
					for (int j = 0; j < psDim; j++) {
						tempVec.push_back(currPair.coefs[i][j]);
					}
				}

				// Add it to the main vector
				toReturn.push_back(tempVec);
			});

	return toReturn;
}

std::vector<std::vector<double> > PSICluster::getCombVector() const {
	// Initial declarations
	std::vector<std::vector<double> > toReturn;

	// Loop on the combining reactants
	std::for_each(combiningReactants.begin(), combiningReactants.end(),
			[&toReturn,this](const CombiningCluster& cc) {
				// Build the vector containing ids and rates
				std::vector<double> tempVec;
				tempVec.push_back(cc.combining.getId() - 1);
				for (int i = 0; i < psDim; i++) {
					tempVec.push_back(cc.coefs[i]);
				}

				// Add it to the main vector
				toReturn.push_back(tempVec);
			});

	return toReturn;
}

std::vector<std::vector<double> > PSICluster::getDissoVector() const {
	// Initial declarations
	std::vector<std::vector<double> > toReturn;

	// Loop on the dissociating pairs
	std::for_each(dissociatingPairs.begin(), dissociatingPairs.end(),
			[&toReturn,this](const ClusterPair& currPair) {
				// Build the vector containing ids and rates
				std::vector<double> tempVec;
				tempVec.push_back(currPair.first.getId() - 1);
				tempVec.push_back(currPair.second.getId() - 1);
				for (int i = 0; i < psDim; i++) {
					for (int j = 0; j < psDim; j++) {
						tempVec.push_back(currPair.coefs[i][j]);
					}
				}

				// Add it to the main vector
				toReturn.push_back(tempVec);
			});

	return toReturn;
}

std::vector<std::vector<double> > PSICluster::getEmitVector() const {
	// Initial declarations
	std::vector<std::vector<double> > toReturn;

	// Loop on the emitting pairs
	std::for_each(emissionPairs.begin(), emissionPairs.end(),
			[&toReturn,this](const ClusterPair& currPair) {
				// Build the vector containing ids and rates
				std::vector<double> tempVec;
				tempVec.push_back(currPair.first.getId() - 1);
				tempVec.push_back(currPair.second.getId() - 1);
				for (int i = 0; i < psDim; i++) {
					for (int j = 0; j < psDim; j++) {
						tempVec.push_back(currPair.coefs[i][j]);
					}
				}

				// Add it to the main vector
				toReturn.push_back(tempVec);
			});

	return toReturn;
}

std::vector<int> PSICluster::getConnectivity() const {
	int connectivityLength = network.getDOF();
	std::vector<int> connectivity = std::vector<int>(connectivityLength, 0);
	auto reactionConnVector = getReactionConnectivity();
	auto dissociationConnVector = getDissociationConnectivity();

	// The reaction and dissociation vectors must have a length equal to the
	// number of clusters
	if (reactionConnVector.size() != (unsigned int) connectivityLength) {
		throw std::string("The reaction vector is an incorrect length");
	}
	if (dissociationConnVector.size() != (unsigned int) connectivityLength) {
		throw std::string("The dissociation vector is an incorrect length");
	}

	// Merge the two vectors such that the final vector contains
	// a 1 at a position if either of the connectivity arrays
	// have a 1
	for (int i = 0; i < connectivityLength; i++) {
		// Consider each connectivity array only if its type is enabled
		connectivity[i] = reactionConnVector[i] || dissociationConnVector[i];
	}

	return connectivity;
}

void PSICluster::dumpCoefficients(std::ostream& os,
		PSICluster::ClusterPair const& curr) const {

	os << "a[0-4][0-4]: ";
	for (int j = 0; j < psDim; j++) {
		for (int i = 0; i < psDim; i++) {
			os << curr.coefs[j][i] << ' ';
		}
	}
}

void PSICluster::dumpCoefficients(std::ostream& os,
		PSICluster::CombiningCluster const& curr) const {

	os << "a[0-4][0-4]: ";
	for (int i = 0; i < psDim; i++) {
		os << curr.coefs[i] << ' ';
	}
}

void PSICluster::outputCoefficientsTo(std::ostream& os) const {

	os << "name: " << name << '\n';
	os << "reacting: " << reactingPairs.size() << '\n';
	std::for_each(reactingPairs.begin(), reactingPairs.end(),
			[this,&os](ClusterPair const& currPair) {
				os << "first: " << currPair.first.getName()
				<< "; second: " << currPair.second.getName()
				<< "; ";
				dumpCoefficients(os, currPair);
				os << '\n';
			});

	os << "combining: " << combiningReactants.size() << '\n';
	std::for_each(combiningReactants.begin(), combiningReactants.end(),
			[this,&os](CombiningCluster const& currCluster) {
				os << "other: " << currCluster.combining.getName()
				<< "; ";
				dumpCoefficients(os, currCluster);
				os << '\n';
			});

	os << "dissociating: " << dissociatingPairs.size() << '\n';
	std::for_each(dissociatingPairs.begin(), dissociatingPairs.end(),
			[this,&os](ClusterPair const& currPair) {
				os << "first: " << currPair.first.getName()
				<< "; second: " << currPair.second.getName()
				<< "; ";
				dumpCoefficients(os, currPair);
				os << '\n';
			});

	os << "emitting: " << emissionPairs.size() << '\n';
	std::for_each(emissionPairs.begin(), emissionPairs.end(),
			[this,&os](ClusterPair const& currPair) {
				os << "first: " << currPair.first.getName()
				<< "; second: " << currPair.second.getName()
				<< "; ";
				dumpCoefficients(os, currPair);
				os << '\n';
			});
}

void PSICluster::useZerothMomentSpecializations() {

	std::for_each(reactingPairs.begin(), reactingPairs.end(),
			[this](ClusterPair& currPair) {
                reactingPairs0.emplace_back(currPair);
			});
#if READY
    reactingPairs.clear();
#endif // READY

	std::for_each(combiningReactants.begin(), combiningReactants.end(),
			[this](CombiningCluster& currCluster) {
                combiningReactants0.emplace_back(currCluster);
			});
#if READY
    combiningReactants.clear();
#endif // READY

	std::for_each(dissociatingPairs.begin(), dissociatingPairs.end(),
			[this](ClusterPair& currPair) {
                dissociatingPairs0.emplace_back(currPair);
			});
#if READY
    dissociatingPairs.clear();
#endif // READY

	std::for_each(emissionPairs.begin(), emissionPairs.end(),
			[this](ClusterPair& currPair) {
                emissionPairs0.emplace_back(currPair);
			});
#if READY
    emissionPairs.clear();
#endif // READY
}

} // namespace xolotlCore

