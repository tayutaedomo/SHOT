/*
 * TaskPrintProblemStats.cpp
 *
 *  Created on: Mar 27, 2015
 *      Author: alundell
 */

#include "TaskPrintProblemStats.h"

TaskPrintProblemStats::TaskPrintProblemStats()
{

}

TaskPrintProblemStats::~TaskPrintProblemStats()
{
	// TODO Auto-generated destructor stub
}

void TaskPrintProblemStats::run()
{
	//TODO: Refactor print stats from problem class
	ProcessInfo::getInstance().originalProblem->printProblemStatistics();
}

std::string TaskPrintProblemStats::getType()
{
	std::string type = typeid(this).name();
	return (type);
}
