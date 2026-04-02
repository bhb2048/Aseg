//
//	SetBubbles.cc - manage bubble placement
//
//	20-dec-11	bhb
//	Modified:
//

#include "Aseg.h"
#include "SetBubbles.h"
#include <sstream>

using namespace std;

SetBubbles::SetBubbles( Aseg *p) : SetBubblesUI(), _parent(p), _activeBubble(-1)
{
	_showApply = false;
}

SetBubbles::~SetBubbles()
{	
}

void
SetBubbles::updateBrowser()
{
	vector<Sphere<uint> > &bubbles = parent()->bubbles();
	uint numBubbles = bubbles.size();

	_activeBubbles->clear();
	for ( uint i=0; i<numBubbles; i++)
	{
		Sphere<uint> &bubble = parent()->bubbles()[i];
		std::ostringstream oss;
		oss << "c=" << bubble.center << ", r=" << bubble.radius;
		_activeBubbles->add( oss.str().c_str());
//		oss << "C=" << ba[i].center << "; ";
//		oss << "R=" << std::setprecision(3) << ba[i].radius;
	}
	if ( activeBubble() >= 0)
		_activeBubbles->value( activeBubble());
	
	parent()->redraw();
}

//
//	UI control functions
//	
void
SetBubbles::addBubble()
{
	Sphere<uint> bubble;

	bubble.center = parent()->cursor();
	bubble.radius = _radiusSlider->value();
	parent()->bubbles().push_back( bubble);
//	cout << "addBubble: " << line.str() << endl;
	_activeBubble = parent()->bubbles().size()-1;
	updateBrowser();
}

void
SetBubbles::removeBubble()
{
	int val = _activeBubbles->value();
//	cout << "removeBubble: " << val << endl;
	vector<Sphere<uint> >::iterator bi = parent()->bubbles().begin();
	parent()->bubbles().erase( bi + val);
	_activeBubble = parent()->bubbles().size()-1;
	updateBrowser();
}

void
SetBubbles::radiusChanged()
{
	if ( activeBubble() < 0)
		return;
	parent()->bubbles()[activeBubble()].radius = _radiusSlider->value();
	
	updateBrowser();
}

void
SetBubbles::activeBubbleChanged()
{
	_activeBubble = _activeBubbles->value();
	parent()->redraw();
}
