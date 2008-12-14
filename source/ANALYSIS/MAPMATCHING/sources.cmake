### the directory name
set(directory source/ANALYSIS/MAPMATCHING)

### list all filenames of the directory here
set(sources_list
BaseGroupFinder.C
BaseSuperimposer.C
DelaunayPairFinder.C
FeatureGroupingAlgorithm.C
FeatureGroupingAlgorithmLabeled.C
FeatureGroupingAlgorithmUnlabeled.C
LabeledPairFinder.C
MapAlignmentAlgorithm.C
MapAlignmentAlgorithmPoseClustering.C
MapAlignmentAlgorithmSpectrumAlignment.C
MapAlignmentAlgorithmApplyGivenTrafo.C
PoseClusteringAffineSuperimposer.C
PoseClusteringShiftSuperimposer.C
SimplePairFinder.C
TransformationDescription.C
)

### add path to the filenames
set(sources)
foreach(i ${sources_list})
	list(APPEND sources ${directory}/${i})
endforeach(i)

### pass source file list to the upper instance
set(OpenMS_sources ${OpenMS_sources} ${sources})

### source group definition
source_group("Source Files\\ANALYSIS\\MAPMATCHING" FILES ${sources})

