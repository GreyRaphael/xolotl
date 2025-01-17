#ifndef LABELPROVIDER_H
#define LABELPROVIDER_H

// Includes
#include <string>

namespace xolotl
{
namespace viz
{
/**
 * LabelProvider provides a series of labels to print on the plots.
 * One Plot must have only one LabelProvider.
 */
class LabelProvider
{
public:
	/**
	 * The label of the X axis of the plot.
	 */
	std::string axis1Label;

	/**
	 * The label of the Y axis of the plot.
	 */
	std::string axis2Label;

	/**
	 * The label of the Z axis of the plot.
	 */
	std::string axis3Label;

	/**
	 * The label for the time steps.
	 */
	std::string axis4Label;

	/**
	 * Title label for the plot.
	 */
	std::string titleLabel;

	/**
	 * Unit label for the plot.
	 */
	std::string unitLabel;

	/**
	 * Time label for the plot.
	 */
	std::string timeLabel;

	/**
	 * Time step label for the plot.
	 */
	std::string timeStepLabel;

	/**
	 * The default constructor
	 */
	LabelProvider() :
		axis1Label(" "),
		axis2Label(" "),
		axis3Label(" "),
		axis4Label(" "),
		titleLabel(" "),
		unitLabel(" "),
		timeLabel(" "),
		timeStepLabel(" ")
	{
	}

	/**
	 * The destructor
	 */
	~LabelProvider()
	{
	}
};

// end class LabelProvider

} /* namespace viz */
} /* namespace xolotl */

#endif
