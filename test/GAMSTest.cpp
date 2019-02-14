/**
   The Supporting Hyperplane Optimization Toolkit (SHOT).

   @author Andreas Lundell, Åbo Akademi University

   @section LICENSE
   This software is licensed under the Eclipse Public License 2.0.
   Please see the README and LICENSE files for more information.
*/

#include "SHOTSolver.h"
#include "ModelingSystemGAMS.h"

using namespace SHOT;

bool ReadProblemGAMS(std::string filename)
{
    bool passed = true;

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    try
    {
        if(solver->setProblem(filename))
        {
            passed = true;
        }
        else
        {
            passed = false;
        }
    }
    catch(ErrorClass& e)
    {
        std::cout << "Error: " << e.errormsg << std::endl;
        return false;
    }

    return passed;
}

bool SolveProblemGAMS(std::string filename)
{
    bool passed = true;

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    try
    {
        if(solver->setProblem(filename))
        {
            passed = true;
        }
        else
        {
            passed = false;
        }
    }
    catch(ErrorClass& e)
    {
        std::cout << "Error: " << e.errormsg << std::endl;
        return false;
    }

    solver->solveProblem();
    std::string osrl = solver->getOSrL();
    std::string trace = solver->getTraceResult();
    if(!SHOT::UtilityFunctions::writeStringToFile("result.osrl", osrl))
    {
        std::cout << "Could not write results to OSrL file." << std::endl;
        passed = false;
    }

    if(!SHOT::UtilityFunctions::writeStringToFile("trace.trc", trace))
    {
        std::cout << "Could not write results to trace file." << std::endl;
        passed = false;
    }

    if(solver->getNumberOfPrimalSolutions() > 0)
    {
        std::cout << std::endl << "Objective value: " << solver->getPrimalSolution().objValue << std::endl;
    }
    else
    {
        passed = false;
    }

    return passed;
}

bool TestRootsearchGAMS(const std::string& problemFile)
{
    bool passed = true;

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    solver->updateSetting("Console.LogLevel", "Output", static_cast<int>(ENUM_OUTPUT_LEVEL_debug));

    env->modelingSystem = std::make_shared<SHOT::ModelingSystemGAMS>(env);
    SHOT::ProblemPtr problem = std::make_shared<SHOT::Problem>(env);

    std::cout << "Reading problem:  " << problemFile << '\n';

    if(std::dynamic_pointer_cast<ModelingSystemGAMS>(env->modelingSystem)
            ->createProblem(problem, problemFile, E_GAMSInputSource::ProblemFile)
        != E_ProblemCreationStatus::NormalCompletion)
    {
        std::cout << "Error while reading problem";
        passed = false;
    }
    else
    {
        env->problem = problem;
        env->reformulatedProblem = env->problem;
        std::cout << "Problem read successfully:\n\n";
        std::cout << env->problem << "\n\n";
    }

    VectorDouble interiorPoint;
    interiorPoint.push_back(7.44902);
    interiorPoint.push_back(8.53506);

    VectorDouble exteriorPoint;
    exteriorPoint.push_back(20.0);
    exteriorPoint.push_back(20.0);

    std::cout << "Interior point:\n";
    UtilityFunctions::displayVector(interiorPoint);

    std::cout << "Exterior point:\n";
    UtilityFunctions::displayVector(exteriorPoint);

    auto rootsearch = std::make_unique<LinesearchMethodBoost>(env);

    auto root = rootsearch->findZero(
        interiorPoint, exteriorPoint, 100, 10e-13, 10e-3, env->problem->nonlinearConstraints, false);

    std::cout << "Root found:\n";
    UtilityFunctions::displayVector(root.first, root.second);

    exteriorPoint.clear();
    exteriorPoint.push_back(8.47199);
    exteriorPoint.push_back(20.0);

    std::cout << "Interior point:\n";
    UtilityFunctions::displayVector(interiorPoint);

    std::cout << "Exterior point:\n";
    UtilityFunctions::displayVector(exteriorPoint);

    root = rootsearch->findZero(
        interiorPoint, exteriorPoint, 100, 10e-13, 10e-3, env->problem->nonlinearConstraints, false);

    std::cout << "Root found:\n";
    UtilityFunctions::displayVector(root.first, root.second);

    exteriorPoint.clear();
    exteriorPoint.push_back(1.0);
    exteriorPoint.push_back(10.0);

    std::cout << "Interior point:\n";
    UtilityFunctions::displayVector(interiorPoint);

    std::cout << "Exterior point:\n";
    UtilityFunctions::displayVector(exteriorPoint);

    root = rootsearch->findZero(
        interiorPoint, exteriorPoint, 100, 10e-13, 10e-3, env->problem->nonlinearConstraints, false);

    std::cout << "Root found:\n";
    UtilityFunctions::displayVector(root.first, root.second);

    return passed;
}

bool TestGradientGAMS(const std::string& problemFile)
{
    bool passed = true;

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    solver->updateSetting("Console.LogLevel", "Output", static_cast<int>(ENUM_OUTPUT_LEVEL_debug));

    env->modelingSystem = std::make_shared<SHOT::ModelingSystemGAMS>(env);
    SHOT::ProblemPtr problem = std::make_shared<SHOT::Problem>(env);

    std::cout << "Reading problem: " << problemFile << '\n';

    if(std::dynamic_pointer_cast<ModelingSystemGAMS>(env->modelingSystem)
            ->createProblem(problem, problemFile, E_GAMSInputSource::ProblemFile)
        != E_ProblemCreationStatus::NormalCompletion)
    {
        std::cout << "Error while reading problem";
        passed = false;
    }
    else
    {
        std::cout << "Problem read successfully:\n\n";
        std::cout << problem << "\n\n";
        std::cout << problem->factorableFunctionsDAG << '\n';
    }

    VectorDouble point;

    for(auto& V : problem->allVariables)
    {
        point.push_back((V->upperBound - V->lowerBound) / 2.0);
    }

    std::cout << "Point to evaluate gradients in:\n";
    UtilityFunctions::displayVector(point);

    for(auto& C : problem->numericConstraints)
    {

        std::cout << "\nCalculating gradient for constraint:\t" << C << ":\n";

        auto gradient = C->calculateGradient(point, true);

        for(auto const& G : gradient)
        {
            std::cout << G.first->name << ":  " << G.second << '\n';
        }

        std::cout << '\n';
    }

    return passed;
}

bool TestReformulateProblemGAMS(const std::string& problemFile)
{
    bool passed = true;

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    solver->updateSetting("Console.LogLevel", "Output", static_cast<int>(ENUM_OUTPUT_LEVEL_debug));

    env->modelingSystem = std::make_shared<SHOT::ModelingSystemGAMS>(env);
    SHOT::ProblemPtr problem = std::make_shared<SHOT::Problem>(env);

    std::cout << "Reading problem: " << problemFile << '\n';

    if(std::dynamic_pointer_cast<ModelingSystemGAMS>(env->modelingSystem)
            ->createProblem(problem, problemFile, E_GAMSInputSource::ProblemFile)
        != E_ProblemCreationStatus::NormalCompletion)
    {
        std::cout << "Error while reading problem";
        passed = false;
    }
    else
    {
        std::cout << "Problem read successfully:\n\n";
        std::cout << problem << "\n\n";
    }

    env->problem = problem;
    auto taskReformulate = std::make_unique<TaskReformulateProblem>(env);

    taskReformulate->run();

    std::cout << env->reformulatedProblem << std::endl;

    return passed;
}

bool TestCallbackGAMS(std::string filename)
{
    bool passed = true;

    std::cout << "The following test will solve a problem, and terminate as soon as the first primal solution has been "
                 "found.\n";

    auto solver = std::make_unique<SHOT::SHOTSolver>();
    auto env = solver->getEnvironment();

    solver->registerCallback(E_EventType::UserTerminationCheck, [&env] {
        std::cout << "Checking whether to terminate SHOT... ";

        if(env->results->primalSolutions.size() > 0)
        {
            env->tasks->terminate();
            std::cout << "Sure, do it.\n";
        }
        else
        {
            std::cout << "Not yet!\n";
        }
    });

    try
    {
        if(solver->setProblem(filename))
        {
            passed = true;
        }
        else
        {
            passed = false;
        }
    }
    catch(ErrorClass& e)
    {
        std::cout << "Error: " << e.errormsg << std::endl;
        return false;
    }

    solver->solveProblem();

    if(env->results->primalSolutions.size() == 1)
        passed = true;
    else
        passed = false;

    return passed;
}

int GAMSTest(int argc, char* argv[])
{
    int defaultchoice = 1;

    int choice = defaultchoice;

    if(argc > 1)
    {
        if(sscanf(argv[1], "%d", &choice) != 1)
        {
            printf("Couldn't parse that input as a number\n");
            return -1;
        }
    }

    bool passed = true;

    switch(choice)
    {
    case 1:
        std::cout << "Starting test to read GAMS files:" << std::endl;
        passed = ReadProblemGAMS("data/tls2.gms");
        std::cout << "Finished test to read GAMS files." << std::endl;
    case 2:
        std::cout << "Starting test to solve a MINLP problem in GAMS syntax:" << std::endl;
        passed = SolveProblemGAMS("data/tls2.gms");
        std::cout << "Finished test to solve a MINLP problem in GAMS syntax." << std::endl;
        break;
    case 3:
        passed = TestRootsearchGAMS("data/shot_ex_jogo.gms");
        break;
    case 4:
        passed = TestGradientGAMS("data/flay02h.gms");
        break;
    case 5:
        passed = TestReformulateProblemGAMS("data/synthes1.gms");
        break;
    case 6:
        passed = TestCallbackGAMS("data/synthes1.gms");
        break;
    default:
        passed = false;
        std::cout << "Test #" << choice << " does not exist!\n";
    }

    if(passed)
        return 0;
    else
        return -1;
}