//
//	SetBubbles.h - manage bubble placement
//
//	20-dec-11  bhb
//	Modified:
//	03-dec-13  bhb	make updateBrowser() public
//	
#ifndef _BUBBLE_H
#define _BUBBLE_H

#include "UI/SetBubblesUI.h"
#include <fltk/gl.h>
#include <Geom/Sphere.h>
#include <list>

class Aseg;

class SetBubbles : public SetBubblesUI
{
	public:
		SetBubbles( Aseg *p);
		virtual ~SetBubbles();

		Aseg *	parent()		{ return _parent;	}
		int		activeBubble()	{ return _activeBubble;	}
		void	updateBrowser();

	private:
		Aseg *						_parent;
		int							_activeBubble;
		
		virtual void addBubble();
		virtual void removeBubble();
		virtual void radiusChanged();
		virtual void activeBubbleChanged();
};
#endif
