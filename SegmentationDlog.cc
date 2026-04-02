//
//	SegmentationDlog.h - subclass for SegmentationUI
//
//	27-dec-11	bhb
//	Modified:
//	01-nov-13	bhb	renamed from 'Segmentation' to 'SegmentationDlog'
//
#include "SegmentationDlog.h"
#include "Aseg.h"
#include <fltk/run.h>		// flush()
#include <sstream>

using namespace std;
using namespace fltk;

SegmentationDlog::SegmentationDlog( Aseg *p) : SegmentationUI(), _parent(p), _stepSize(1)
{
	
}

SegmentationDlog::~SegmentationDlog()
{
	
}

void SegmentationDlog::restartSegmentation()
{
	textOutput()->value( "Restart");	flush();
	parent()->restartSegmentation();
}

void SegmentationDlog::startSegmentation()
{
	textOutput()->value( "Running");	flush();
	parent()->startSegmentation();
}

void SegmentationDlog::stopSegmentation()
{
	textOutput()->value( "Stopping");	flush();
	parent()->stopSegmentation();
}

void SegmentationDlog::stepSegmentation()
{
	textOutput()->value( "Running");	flush();
	parent()->stepSegmentation();
	textOutput()->value( "Ready");	flush();
}

void SegmentationDlog::setStepSize()
{
	uint stepSize[] = { 1, 2, 5, 10, 20, 50};
	
	_stepSize = stepSize[_stepSizeChoice->value()];
}

