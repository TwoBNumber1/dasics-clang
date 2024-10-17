add_library(LLVMAnderson MODULE
        AndersonPointsToAnalysis.h
        AndersonPointsToAnalysis.cpp
        PointerAssignment.cpp
        PointsToSolver.cpp
        PointsToSolver.h
        ValueTree.cpp
        ValueTreeNode.cpp
        PluginRegistration.cpp
        )