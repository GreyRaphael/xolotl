#pragma once

#include <xolotl/core/Constants.h>
#include <xolotl/util/MathUtils.h>

namespace xolotl
{
namespace core
{
namespace network
{
KOKKOS_INLINE_FUNCTION
bool
ZrClusterGenerator::refine(const Region& region, BoolArray& result) const
{
	result[0] = true;
	result[1] = true;
	result[2] = true;

	// No need for refine here because we are not using grouping

	return true;
}

KOKKOS_INLINE_FUNCTION
bool
ZrClusterGenerator::select(const Region& region) const
{
	// adding basal
	int nAxis = (region[Species::V].begin() > 0) +
		(region[Species::I].begin() > 0) + (region[Species::Basal].begin() > 0);

	/*
	int nAxis =
		(region[Species::V].begin() > 0) + (region[Species::I].begin() > 0);
	*/

	if (nAxis > 1) {
		return false;
	}

	if (region.isSimplex()) {
		// Each cluster should be on one axis and one axis only
		if (nAxis != 1) {
			return false;
		}

		// I
		if (region[Species::I].begin() > _maxI)
			return false;

		// V
		if (region[Species::V].begin() > _maxV)
			return false;

		// adding basal
		// Basal
		if (region[Species::Basal].begin() > _maxV)
			return false;
	}

	return true;
}

template <typename PlsmContext>
KOKKOS_INLINE_FUNCTION
double
ZrClusterGenerator::getFormationEnergy(
	const Cluster<PlsmContext>& cluster) const noexcept
{
	const auto& reg = cluster.getRegion();
	Composition lo(reg.getOrigin());
	double energy = 0.0;

	// TODO: fix the formula for V and I

	if (lo.isOnAxis(Species::V)) {
		for (auto j : makeIntervalRange(reg[Species::V])) {
			energy += 0.0 + 0.0 * (pow((double)j, 2.0 / 3.0) - 1.0);
		}
		return energy / reg[Species::V].length();
	}
	if (lo.isOnAxis(Species::I)) {
		for (auto j : makeIntervalRange(reg[Species::I])) {
			energy += 0.0 + 0.0 * (pow((double)j, 2.0 / 3.0) - 1.0);
		}
		return energy / reg[Species::I].length();
	}
	return 0.0;
}

template <typename PlsmContext>
KOKKOS_INLINE_FUNCTION
double
ZrClusterGenerator::getMigrationEnergy(
	const Cluster<PlsmContext>& cluster) const noexcept
{
	constexpr double defaultMigration = -1.0;
	constexpr double iNineMigration = 0.10;

	const auto& reg = cluster.getRegion();
	Composition comp(reg.getOrigin());
	double migrationEnergy = util::infinity<double>;

	if (comp.isOnAxis(Species::V) && comp[Species::V] <= 6) {
		return defaultMigration;
	}
	if (comp.isOnAxis(Species::I)) {
		if (comp[Species::I] <= 3)
			return defaultMigration;
		if (comp[Species::I] == 9)
			return iNineMigration;
	}

	return migrationEnergy;
}

template <typename PlsmContext>
KOKKOS_INLINE_FUNCTION
double
ZrClusterGenerator::getDiffusionFactor(
	const Cluster<PlsmContext>& cluster, double latticeParameter) const noexcept
{
	constexpr double defaultDiffusion = 1.0;
	// constexpr double iNineDiffusion = 2.2e+11;
	constexpr double iNineDiffusion = 0.0;

	const auto& reg = cluster.getRegion();
	Composition comp(reg.getOrigin());
	double diffusionFactor = 0.0;

	if (comp.isOnAxis(Species::V) && comp[Species::V] <= 6) {
		return defaultDiffusion;
	}

	if (comp.isOnAxis(Species::I)) {
		if (comp[Species::I] <= 3)
			return defaultDiffusion;
		if (comp[Species::I] == 9)
			return iNineDiffusion;
	}

	return diffusionFactor;
}

template <typename PlsmContext>
KOKKOS_INLINE_FUNCTION
double
ZrClusterGenerator::getReactionRadius(const Cluster<PlsmContext>& cluster,
	double latticeParameter, double interstitialBias,
	double impurityRadius) const noexcept
{
	const double prefactor = 0.0 * latticeParameter * latticeParameter *
		latticeParameter / ::xolotl::core::pi;
	const auto& reg = cluster.getRegion();
	Composition lo(reg.getOrigin());
	double radius = 0.0;
	int basalTransitionSize = 91;

	// jmr: rn = (3nOmega/4pi)^1/3 [nm] for n < 10
	// jmr: Note that (3Omega/4pi) = 5.586e-3 nm^3, where Omega = 0.0234 nm^3
	// jmr: For prismatic loops (n > 9): rn = 0.163076*sqrt(n) [nm]
	// jmr: For basal loops (n > 9): rn = 0.169587*sqrt(n) [nm]

	if (lo.isOnAxis(Species::V)) {
		for (auto j : makeIntervalRange(reg[Species::V])) {
			if (lo[Species::V] < 10)
				radius += pow(5.586e-3 * (double)j, 1.0 / 3.0);
			else
				radius += 0.163076 * pow((double)j, 0.5);
		}
		return radius / reg[Species::V].length();
	}

	// adding basal
	if (lo.isOnAxis(Species::Basal)) {
		for (auto j : makeIntervalRange(reg[Species::Basal])) {
			// Treat the case for faulted basal pyramids
			// Estimate a spherical radius based on equivalent surface area
			if (lo[Species::Basal] < basalTransitionSize) {
				double Sb = pow(3, 0.5) / 2 * pow(3.232, 2) *
					(double)j; // Basal surface area
				double Sp = 3.232 / 2 *
					pow(3 * pow(3.232, 2) + 4 * pow(5.17, 2), 0.5) *
					(double)j; // Prismatic surface area
				radius += pow((Sb + Sp) / (4 * pi), 0.5) / 10;
			}

			// Treat the case of a basal c-loop
			else
				radius += 0.169587 * pow((double)j, 0.5);
		}
		return radius / reg[Species::Basal].length();
	}

	if (lo.isOnAxis(Species::I)) {
		for (auto j : makeIntervalRange(reg[Species::I])) {
			if (lo[Species::I] < 10)
				radius += pow(5.586e-3 * (double)j, 1.0 / 3.0);
			else
				radius += 0.163076 * pow((double)j, 0.5);
		}
		return radius / reg[Species::I].length();
	}
	return radius;
}
} // namespace network
} // namespace core
} // namespace xolotl
