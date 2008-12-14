### the directory name
set(directory include/OpenMS/DATASTRUCTURES)

### list all header files of the directory here
set(sources_list_h
BigString.h
ConstRefVector.h
ConvexHull2D.h
DBoundingBox.h
DIntervalBase.h
DPosition.h
DRange.h
DataValue.h
Date.h
DateTime.h
DefaultParamHandler.h
DistanceMatrix.h
DoubleList.h
IntList.h
IsotopeCluster.h
Map.h
Matrix.h
Param.h
SparseVector.h
String.h
StringList.h
SuffixArray.h
SuffixArrayPeptideFinder.h
SuffixArraySeqan.h
SuffixArrayTrypticCompressed.h
SuffixArrayTrypticSeqan.h
)

### add path to the filenames
set(sources_h)
foreach(i ${sources_list_h})
	list(APPEND sources_h ${directory}/${i})
endforeach(i)

### source group definition
source_group("Header Files\\OpenMS\\DATASTRUCTURES" FILES ${sources_h})

set(OpenMS_sources_h ${OpenMS_sources_h} ${sources_h})

