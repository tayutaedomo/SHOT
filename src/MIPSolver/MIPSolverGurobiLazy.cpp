/**
   The Supporting Hyperplane Optimization Toolkit (SHOT).

   @author Andreas Lundell, Åbo Akademi University

   @section LICENSE
   This software is licensed under the Eclipse Public License 2.0.
   Please see the README and LICENSE files for more information.
*/

#include "MIPSolverGurobiLazy.h"

namespace SHOT
{

MIPSolverGurobiLazy::MIPSolverGurobiLazy(EnvironmentPtr envPtr)
{
    env = envPtr;
    discreteVariablesActivated = true;

    try
    {
        gurobiEnv = std::make_unique<GRBEnv>();
        gurobiModel = std::make_unique<GRBModel>(gurobiEnv.get());
    }
    catch(GRBException& e)
    {
        {
            env->output->outputError("Error when initializing Gurobi:", e.getMessage());
        }

        return;
    }

    cachedSolutionHasChanged = true;
    isVariablesFixed = false;

    checkParameters();
}

MIPSolverGurobiLazy::~MIPSolverGurobiLazy() {}

void MIPSolverGurobiLazy::initializeSolverSettings()
{
    MIPSolverGurobi::initializeSolverSettings();

    try
    {
        gurobiModel->set(GRB_IntParam_LazyConstraints, 1);
    }
    catch(GRBException& e)
    {
        env->output->outputError("Error when initializing parameters for linear solver", e.getMessage());
    }
}

int MIPSolverGurobiLazy::increaseSolutionLimit(int increment)
{
    gurobiModel->getEnv().set(
        GRB_IntParam_SolutionLimit, gurobiModel->getEnv().get(GRB_IntParam_SolutionLimit) + increment);

    return (gurobiModel->getEnv().get(GRB_IntParam_SolutionLimit));
}

void MIPSolverGurobiLazy::setSolutionLimit(long limit)
{
    if(limit > GRB_MAXINT)
        gurobiModel->getEnv().set(GRB_IntParam_SolutionLimit, GRB_MAXINT);
    else
        gurobiModel->getEnv().set(GRB_IntParam_SolutionLimit, limit);
}

int MIPSolverGurobiLazy::getSolutionLimit() { return (gurobiModel->getEnv().get(GRB_IntParam_SolutionLimit)); }

void MIPSolverGurobiLazy::checkParameters() {}

E_ProblemSolutionStatus MIPSolverGurobiLazy::solveProblem()
{
    E_ProblemSolutionStatus MIPSolutionStatus;
    cachedSolutionHasChanged = true;

    try
    {
        GurobiCallback gurobiCallback = GurobiCallback(gurobiModel->getVars(), env);
        gurobiModel->setCallback(&gurobiCallback);
        gurobiModel->optimize();

        MIPSolutionStatus = getSolutionStatus();
    }
    catch(GRBException& e)
    {
        env->output->outputError("Error when solving MIP/LP problem", e.getMessage());
        MIPSolutionStatus = E_ProblemSolutionStatus::Error;
    }

    return (MIPSolutionStatus);
}

void GurobiCallback::callback()
{
    if(where == GRB_CB_POLLING || where == GRB_CB_PRESOLVE || where == GRB_CB_SIMPLEX || where == GRB_CB_MESSAGE
        || where == GRB_CB_BARRIER)
        return;

    try
    {
        // Check if better dual bound
        double tmpDualObjBound;

        if(where == GRB_CB_MIP || where == GRB_CB_MIPSOL || where == GRB_CB_MIPNODE)
        {
            switch(where)
            {
            case GRB_CB_MIP:
                tmpDualObjBound = getDoubleInfo(GRB_CB_MIP_OBJBND);
                break;
            case GRB_CB_MIPSOL:
                tmpDualObjBound = getDoubleInfo(GRB_CB_MIPSOL_OBJBND);
                break;
            case GRB_CB_MIPNODE:
                tmpDualObjBound = getDoubleInfo(GRB_CB_MIPNODE_OBJBND);
                break;
            default:
                break;
            }

            if((isMinimization && tmpDualObjBound > env->results->getDualBound())
                || (!isMinimization && tmpDualObjBound < env->results->getDualBound()))
            {
                VectorDouble doubleSolution; // Empty since we have no point

                DualSolution sol = { doubleSolution, E_DualSolutionSource::MIPSolverBound, tmpDualObjBound,
                    env->results->getCurrentIteration()->iterationNumber };
                env->dualSolver->addDualSolutionCandidate(sol);
            }
        }

        if(where == GRB_CB_MIPSOL)
        {
            // Check for new primal solution
            double tmpPrimalObjBound = getDoubleInfo(GRB_CB_MIPSOL_OBJ);

            if((tmpPrimalObjBound < 1e100)
                && ((isMinimization && tmpPrimalObjBound < env->results->getPrimalBound())
                       || (!isMinimization && tmpPrimalObjBound > env->results->getPrimalBound())))
            {
                VectorDouble primalSolution(numVar);

                for(int i = 0; i < numVar; i++)
                {
                    primalSolution.at(i) = getSolution(vars[i]);
                }

                SolutionPoint tmpPt;

                if(env->problem->properties.numberOfNonlinearConstraints > 0)
                {
                    auto maxDev = env->problem->getMaxNumericConstraintValue(
                        primalSolution, env->problem->nonlinearConstraints);
                    tmpPt.maxDeviation = PairIndexValue(maxDev.constraint->index, maxDev.normalizedValue);
                }

                tmpPt.iterFound = env->results->getCurrentIteration()->iterationNumber;
                tmpPt.objectiveValue = env->problem->objectiveFunction->calculateValue(primalSolution);
                tmpPt.point = primalSolution;

                env->primalSolver->addPrimalSolutionCandidate(tmpPt, E_PrimalSolutionSource::LazyConstraintCallback);
            }
        }

        if(env->results->isAbsoluteObjectiveGapToleranceMet() || env->results->isRelativeObjectiveGapToleranceMet()
            || checkIterationLimit())
        {
            abort();
            return;
        }

        if(where == GRB_CB_MIPNODE && getIntInfo(GRB_CB_MIPNODE_STATUS) == GRB_OPTIMAL)
        {
            if(env->results->getCurrentIteration()->relaxedLazyHyperplanesAdded
                < env->settings->getIntSetting("Relaxation.MaxLazyConstraints", "Dual"))
            {
                int waitingListSize = env->dualSolver->MIPSolver->hyperplaneWaitingList.size();
                std::vector<SolutionPoint> solutionPoints(1);

                VectorDouble solution(numVar);

                for(int i = 0; i < numVar; i++)
                {
                    solution.at(i) = getNodeRel(vars[i]);
                }

                SolutionPoint tmpSolPt;

                if(env->problem->properties.numberOfNonlinearConstraints > 0)
                {
                    auto maxDev = env->reformulatedProblem->getMaxNumericConstraintValue(
                        solution, env->reformulatedProblem->nonlinearConstraints);
                    tmpSolPt.maxDeviation = PairIndexValue(maxDev.constraint->index, maxDev.normalizedValue);
                }

                tmpSolPt.point = solution;
                tmpSolPt.objectiveValue = env->reformulatedProblem->objectiveFunction->calculateValue(solution);
                tmpSolPt.iterFound = env->results->getCurrentIteration()->iterationNumber;

                solutionPoints.at(0) = tmpSolPt;

                if(static_cast<ES_HyperplaneCutStrategy>(env->settings->getIntSetting("CutStrategy", "Dual"))
                    == ES_HyperplaneCutStrategy::ESH)
                {
                    static_cast<TaskSelectHyperplanePointsESH*>(taskSelectHPPts.get())->run(solutionPoints);
                }
                else
                {
                    static_cast<TaskSelectHyperplanePointsECP*>(taskSelectHPPts.get())->run(solutionPoints);
                }

                env->results->getCurrentIteration()->relaxedLazyHyperplanesAdded
                    += (env->dualSolver->MIPSolver->hyperplaneWaitingList.size() - waitingListSize);
            }
        }

        if(where == GRB_CB_MIPSOL)
        {
            auto currIter = env->results->getCurrentIteration();

            if(currIter->isSolved)
            {
                env->results->createIteration();
                currIter = env->results->getCurrentIteration();
            }

            VectorDouble solution(numVar);

            for(int i = 0; i < numVar; i++)
            {
                solution.at(i) = getSolution(vars[i]);
            }

            SolutionPoint solutionCandidate;

            if(env->reformulatedProblem->properties.numberOfNonlinearConstraints > 0)
            {
                auto maxDev = env->reformulatedProblem->getMaxNumericConstraintValue(
                    solution, env->reformulatedProblem->nonlinearConstraints);

                // Remove??
                if(maxDev.normalizedValue <= env->settings->getDoubleSetting("ConstraintTolerance", "Termination"))
                {
                    return;
                }

                solutionCandidate.maxDeviation = PairIndexValue(maxDev.constraint->index, maxDev.normalizedValue);
            }

            solutionCandidate.point = solution;
            solutionCandidate.objectiveValue = getDoubleInfo(GRB_CB_MIPSOL_OBJ);
            solutionCandidate.iterFound = env->results->getCurrentIteration()->iterationNumber;

            std::vector<SolutionPoint> candidatePoints(1);
            candidatePoints.at(0) = solutionCandidate;

            addLazyConstraint(candidatePoints);

            currIter->solutionStatus = E_ProblemSolutionStatus::Feasible;
            currIter->objectiveValue = getDoubleInfo(GRB_CB_MIPSOL_OBJ);

            currIter->numberOfExploredNodes = lastExploredNodes - env->solutionStatistics.numberOfExploredNodes;
            env->solutionStatistics.numberOfExploredNodes = lastExploredNodes;
            currIter->numberOfOpenNodes = lastOpenNodes;

            auto bounds = std::make_pair(env->results->getDualBound(), env->results->getPrimalBound());
            currIter->currentObjectiveBounds = bounds;

            if(env->settings->getBoolSetting("Linesearch.Use", "Primal")
                && env->reformulatedProblem->properties.numberOfNonlinearConstraints > 0)
            {
                taskSelectPrimalSolutionFromLinesearch.get()->run(candidatePoints);
            }

            if(checkFixedNLPStrategy(candidatePoints.at(0)))
            {
                env->primalSolver->addFixedNLPCandidate(candidatePoints.at(0).point, E_PrimalNLPSource::FirstSolution,
                    getDoubleInfo(GRB_CB_MIPSOL_OBJ), env->results->getCurrentIteration()->iterationNumber,
                    candidatePoints.at(0).maxDeviation);

                tSelectPrimNLP.get()->run();

                env->primalSolver->checkPrimalSolutionCandidates();
            }

            if(env->settings->getBoolSetting("HyperplaneCuts.UseIntegerCuts", "Dual"))
            {
                bool addedIntegerCut = false;

                for(auto& ic : env->dualSolver->MIPSolver->integerCutWaitingList)
                {
                    this->createIntegerCut(ic.first, ic.second);
                    addedIntegerCut = true;
                }

                if(addedIntegerCut)
                {
                    env->output->outputInfo("        Added "
                        + std::to_string(env->dualSolver->MIPSolver->integerCutWaitingList.size())
                        + " integer cut(s).                                        ");
                }

                env->dualSolver->MIPSolver->integerCutWaitingList.clear();
            }

            currIter->isSolved = true;

            auto threadId = "";
            printIterationReport(candidatePoints.at(0), threadId);

            if(env->results->isAbsoluteObjectiveGapToleranceMet() || env->results->isRelativeObjectiveGapToleranceMet())
            {
                abort();
                return;
            }
        }

        if(where == GRB_CB_MIP)
        {
            lastExploredNodes = (int)getDoubleInfo(GRB_CB_MIP_NODCNT);
            lastOpenNodes = (int)getDoubleInfo(GRB_CB_MIP_NODLFT);
        }

        if(where == GRB_CB_MIPSOL)
        {
            // Add current primal bound as new incumbent candidate
            auto primalBound = env->results->getPrimalBound();

            if(((isMinimization && lastUpdatedPrimal < primalBound) || (!isMinimization && primalBound > primalBound)))
            {
                auto primalSol = env->results->primalSolution;

                VectorDouble primalSolution(numVar);

                for(int i = 0; i < primalSol.size(); i++)
                {
                    setSolution(vars[i], primalSol.at(i));
                }

                if(env->dualSolver->MIPSolver->hasAuxilliaryObjectiveVariable())
                {
                    setSolution(vars[numVar - 1], env->results->getPrimalBound());
                }

                if(env->dualSolver->MIPSolver->hasAuxilliaryObjectiveVariable())
                {
                    tmpVals.add(env->results->getPrimalBound());
                }

                lastUpdatedPrimal = primalBound;
            }

            // Adds cutoff

            double cutOffTol = env->settings->getDoubleSetting("MIP.CutOffTolerance", "Dual");

            if(isMinimization)
            {
                static_cast<MIPSolverGurobiLazy*>(env->dualSolver->MIPSolver.get())
                    ->gurobiModel->set(GRB_DoubleParam_Cutoff, primalBound + cutOffTol);

                env->output->outputInfo("     Setting cutoff value to "
                    + UtilityFunctions::toString(primalBound + cutOffTol) + " for minimization.");
            }
            else
            {
                static_cast<MIPSolverGurobiLazy*>(env->dualSolver->MIPSolver.get())
                    ->gurobiModel->set(GRB_DoubleParam_Cutoff, -primalBound - cutOffTol);

                env->output->outputInfo("     Setting cutoff value to "
                    + UtilityFunctions::toString(-primalBound - cutOffTol) + " for minimization.");
            }
        }
    }
    catch(GRBException& e)
    {
        env->output->outputError("Gurobi error when running main callback method", e.getMessage());
    }
}

void GurobiCallback::createHyperplane(Hyperplane hyperplane)
{
    try
    {
        auto currIter = env->results->getCurrentIteration(); // The unsolved new iteration
        auto optional = env->dualSolver->MIPSolver->createHyperplaneTerms(hyperplane);

        if(!optional)
        {
            return;
        }

        auto tmpPair = optional.get();

        bool hyperplaneIsOk = true;

        for(auto& E : tmpPair.first)
        {
            if(E.value != E.value) // Check for NaN
            {
                env->output->outputError("     Warning: hyperplane for constraint "
                    + std::to_string(hyperplane.sourceConstraint->index)
                    + " not generated, NaN found in linear terms for variable "
                    + env->problem->getVariable(E.index)->name);
                hyperplaneIsOk = false;

                break;
            }
        }

        if(hyperplaneIsOk)
        {
            GeneratedHyperplane genHyperplane;

            GRBLinExpr expr = 0;

            for(int i = 0; i < tmpPair.first.size(); i++)
            {
                expr += +(tmpPair.first.at(i).value) * (vars[tmpPair.first.at(i).index]);
            }

            addLazy(expr <= -tmpPair.second);

            int constrIndex = 0;
            genHyperplane.generatedConstraintIndex = constrIndex;
            genHyperplane.sourceConstraintIndex = hyperplane.sourceConstraintIndex;
            genHyperplane.generatedPoint = hyperplane.generatedPoint;
            genHyperplane.source = hyperplane.source;
            genHyperplane.generatedIter = currIter->iterationNumber;
            genHyperplane.isLazy = false;
            genHyperplane.isRemoved = false;

            // env->dualSolver->MIPSolver->generatedHyperplanes.push_back(genHyperplane);

            currIter->numHyperplanesAdded++;
            currIter->totNumHyperplanes++;
        }
    }
    catch(GRBException& e)
    {
        env->output->outputError("Gurobi error when creating lazy hyperplane", e.getMessage());
    }
}

GurobiCallback::GurobiCallback(GRBVar* xvars, EnvironmentPtr envPtr)
{
    env = envPtr;
    vars = xvars;

    isMinimization = env->reformulatedProblem->objectiveFunction->properties.isMinimize;

    env->solutionStatistics.iterationLastLazyAdded = 0;

    cbCalls = 0;

    if(static_cast<ES_HyperplaneCutStrategy>(env->settings->getIntSetting("CutStrategy", "Dual"))
        == ES_HyperplaneCutStrategy::ESH)
    {
        tUpdateInteriorPoint = std::shared_ptr<TaskUpdateInteriorPoint>(std::make_shared<TaskUpdateInteriorPoint>(env));

        taskSelectHPPts
            = std::shared_ptr<TaskSelectHyperplanePointsESH>(std::make_shared<TaskSelectHyperplanePointsESH>(env));
    }
    else
    {
        taskSelectHPPts
            = std::shared_ptr<TaskSelectHyperplanePointsECP>(std::make_shared<TaskSelectHyperplanePointsECP>(env));
    }

    tSelectPrimNLP
        = std::shared_ptr<TaskSelectPrimalCandidatesFromNLP>(std::make_shared<TaskSelectPrimalCandidatesFromNLP>(env));

    if(env->reformulatedProblem->objectiveFunction->properties.classification
        > E_ObjectiveFunctionClassification::Quadratic)
    {
        taskSelectHPPtsByObjectiveLinesearch = std::shared_ptr<TaskSelectHyperplanePointsByObjectiveLinesearch>(
            std::make_shared<TaskSelectHyperplanePointsByObjectiveLinesearch>(env));
    }

    if(env->settings->getBoolSetting("Linesearch.Use", "Primal")
        && env->reformulatedProblem->properties.numberOfNonlinearConstraints > 0)
    {
        taskSelectPrimalSolutionFromLinesearch = std::shared_ptr<TaskSelectPrimalCandidatesFromLinesearch>(
            std::make_shared<TaskSelectPrimalCandidatesFromLinesearch>(env));
    }

    lastUpdatedPrimal = env->results->getPrimalBound();

    numVar
        = (static_cast<MIPSolverGurobiLazy*>(env->dualSolver->MIPSolver.get()))->gurobiModel->get(GRB_IntAttr_NumVars);
}

void GurobiCallback::createIntegerCut(VectorInteger& binaryIndexes)
{
    try
    {
        GRBLinExpr expr = 0;

        for(int i = 0; i < binaryIndexes.size(); i++)
        {
            expr += vars[binaryIndexes.at(i)];
        }

        addLazy(expr <= binaryIndexes.size() - 1.0);

        env->solutionStatistics.numberOfIntegerCuts++;
    }
    catch(GRBException& e)
    {
        env->output->outputError("Gurobi error when adding lazy integer cut", e.getMessage());
    }
}

void GurobiCallback::addLazyConstraint(std::vector<SolutionPoint> candidatePoints)
{
    try
    {
        this->cbCalls++;

        if(static_cast<ES_HyperplaneCutStrategy>(env->settings->getIntSetting("CutStrategy", "Dual"))
            == ES_HyperplaneCutStrategy::ESH)
        {
            tUpdateInteriorPoint->run();

            static_cast<TaskSelectHyperplanePointsESH*>(taskSelectHPPts.get())->run(candidatePoints);

            if(env->reformulatedProblem->objectiveFunction->properties.classification
                > E_ObjectiveFunctionClassification::Quadratic)
            {
                taskSelectHPPtsByObjectiveLinesearch->run(candidatePoints);
            }
        }
        else
        {
            static_cast<TaskSelectHyperplanePointsECP*>(taskSelectHPPts.get())->run(candidatePoints);

            if(env->reformulatedProblem->objectiveFunction->properties.classification
                > E_ObjectiveFunctionClassification::Quadratic)
            {
                taskSelectHPPtsByObjectiveLinesearch->run(candidatePoints);
            }
        }

        for(auto& hp : env->dualSolver->MIPSolver->hyperplaneWaitingList)
        {
            this->createHyperplane(hp);
            this->lastNumAddedHyperplanes++;
        }

        env->dualSolver->MIPSolver->hyperplaneWaitingList.clear();
    }
    catch(GRBException& e)
    {
        env->output->outputError("Gurobi error when invoking adding lazy constraint", e.getMessage());
    }
}
} // namespace SHOT