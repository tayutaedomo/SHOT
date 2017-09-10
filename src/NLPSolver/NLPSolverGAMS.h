/**
 * @file NLPSolverGAMS.h
 */

#pragma once
#include "NLPSolverBase.h"

#include "gmomcc.h"
#include "gevmcc.h"

class NLPSolverGAMS: public NLPSolverBase
{
	private:
		gmoHandle_t gmo;
		gevHandle_t gev;

	public:
		NLPSolverGAMS();

		~NLPSolverGAMS()
		{
		}
		;

		void setStartingPoint(std::vector<int> variableIndexes, std::vector<double> variableValues);
		void clearStartingPoint();

		void fixVariables(std::vector<int> variableIndexes, std::vector<double> variableValues);

		void unfixVariables();

		void saveOptionsToFile(std::string fileName);

		std::vector<double> getSolution();
		double getSolution(int i);
		double getObjectiveValue();

		bool isObjectiveFunctionNonlinear();
		int getObjectiveFunctionVariableIndex();

	protected:
		E_NLPSolutionStatus solveProblemInstance();
		bool createProblemInstance(OSInstance * origInstance);

		std::vector<double> getCurrentVariableLowerBounds();
		std::vector<double> getCurrentVariableUpperBounds();

//	 void setSolverSpecificInitialSettings();

	private:

//	
//	ProcessInfo *processInfo;

};
