/*
 * TaskSelectHyperplanePointsSolution.cpp
 *
 *  Created on: Mar 28, 2015
 *      Author: alundell
 */

#include "TaskSelectHyperplanePointsSolution.h"

TaskSelectHyperplanePointsSolution::TaskSelectHyperplanePointsSolution()
{

}

TaskSelectHyperplanePointsSolution::~TaskSelectHyperplanePointsSolution()
{

}

void TaskSelectHyperplanePointsSolution::run()
{
	this->run(ProcessInfo::getInstance().getPreviousIteration()->solutionPoints);
}

void TaskSelectHyperplanePointsSolution::run(vector<SolutionPoint> solPoints)
{
	int addedHyperplanes = 0;

	auto currIter = ProcessInfo::getInstance().getCurrentIteration(); // The unsolved new iteration

	auto originalProblem = ProcessInfo::getInstance().originalProblem;

	auto constrSelFactor = Settings::getInstance().getDoubleSetting("LinesearchConstraintSelectionFactor", "ECP");

	for (int i = 0; i < solPoints.size(); i++)
	{
		auto tmpMostDevConstrs = originalProblem->getMostDeviatingConstraints(solPoints.at(i).point, constrSelFactor);

		for (int j = 0; j < tmpMostDevConstrs.size(); j++)
		{
			if (addedHyperplanes >= Settings::getInstance().getIntSetting("MaxHyperplanesPerIteration", "Algorithm")) return;

			if (tmpMostDevConstrs.at(j).value < 0)
			{
				ProcessInfo::getInstance().outputWarning("LP point is in the interior!");
			}
			else
			{
				Hyperplane hyperplane;
				hyperplane.sourceConstraintIndex = tmpMostDevConstrs.at(j).idx;
				hyperplane.generatedPoint = solPoints.at(i).point;

				if (i == 0 && currIter->isMILP())
				{
					hyperplane.source = E_HyperplaneSource::MIPOptimalSolutionPoint;
				}
				else if (currIter->isMILP())
				{
					hyperplane.source = E_HyperplaneSource::MIPSolutionPoolSolutionPoint;
				}
				else
				{
					hyperplane.source = E_HyperplaneSource::LPRelaxedSolutionPoint;
				}

				ProcessInfo::getInstance().hyperplaneWaitingList.push_back(hyperplane);

				addedHyperplanes++;
			}
		}
	}
}

std::string TaskSelectHyperplanePointsSolution::getType()
{
	std::string type = typeid(this).name();
	return (type);

}
