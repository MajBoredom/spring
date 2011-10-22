/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <cassert>
#include <cmath>
#include <limits>

#include "Node.hpp"
#include "NodeLayer.hpp"
#include "PathManager.hpp"

#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Rectangle.h"



void QTPFS::INode::SetPathCost(unsigned int type, float cost) {
	switch (type) {
		case NODE_PATH_COST_F: { fCost = cost; return; } break;
		case NODE_PATH_COST_G: { gCost = cost; return; } break;
		case NODE_PATH_COST_H: { hCost = cost; return; } break;
	}

	assert(false);
}

float QTPFS::INode::GetPathCost(unsigned int type) const {
	switch (type) {
		case NODE_PATH_COST_F: { return fCost; } break;
		case NODE_PATH_COST_G: { return gCost; } break;
		case NODE_PATH_COST_H: { return hCost; } break;
	}

	assert(false);
	return 0.0f;
}

float QTPFS::INode::GetDistance(const INode* n, unsigned int type) const {
	const float dx = float(xmid() * SQUARE_SIZE) - float(n->xmid() * SQUARE_SIZE);
	const float dz = float(zmid() * SQUARE_SIZE) - float(n->zmid() * SQUARE_SIZE);

	switch (type) {
		case NODE_DIST_EUCLIDEAN: { return math::sqrt((dx * dx) + (dz * dz)); } break;
		case NODE_DIST_MANHATTAN: { return (math::fabs(dx) + math::fabs(dz)); } break;
	}

	return -1.0f;
}

unsigned int QTPFS::INode::GetNeighborRelation(const INode* ngb) const {
	const bool edgeL = (xmin() == ngb->xmax());
	const bool edgeR = (xmax() == ngb->xmin());
	const bool edgeT = (zmin() == ngb->zmax());
	const bool edgeB = (zmax() == ngb->zmin());

	unsigned int rel = 0;

	if (edgeL) { rel |= REL_NGB_EDGE_L; }
	if (edgeR) { rel |= REL_NGB_EDGE_R; }
	if (edgeT) { rel |= REL_NGB_EDGE_T; }
	if (edgeB) { rel |= REL_NGB_EDGE_B; }

	return rel;
}

unsigned int QTPFS::INode::GetRectangleRelation(const SRectangle& r) const {
	// NOTE: we consider "interior" to be the set of all
	// legal indices, and conversely "exterior" the set
	// of all illegal indices (min-edges are inclusive,
	// max-edges are exclusive)
	//
	if ((r.x1 >= xmin() && r.x2 <  xmax()) && (r.z1 >= zmin() && r.z2 <  zmax())) { return REL_RECT_INTERIOR_NODE; }
	if ((r.x1 >= xmax() || r.x2 <  xmin()) || (r.z1 >= zmax() || r.z2 <  zmin())) { return REL_RECT_EXTERIOR_NODE; }
	if ((r.x1 <  xmin() && r.x2 >= xmax()) && (r.z1 <  zmin() && r.z2 >= zmax())) { return REL_NODE_INTERIOR_RECT; }

	return REL_NODE_OVERLAPS_RECT;
}

float3 QTPFS::INode::GetNeighborEdgeMidPoint(const INode* ngb) const {
	float3 p = ZeroVector;

	const unsigned int
		minx = std::max(xmin(), ngb->xmin()),
		maxx = std::min(xmax(), ngb->xmax());
	const unsigned int
		minz = std::max(zmin(), ngb->zmin()),
		maxz = std::min(zmax(), ngb->zmax());
	const unsigned int
		midx = (maxx + minx) >> 1,
		midz = (maxz + minz) >> 1;

	switch (GetNeighborRelation(ngb)) {
		// corners
		case REL_NGB_EDGE_T | REL_NGB_EDGE_L: { p.x = xmin() * SQUARE_SIZE;  p.z = zmin() * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_T | REL_NGB_EDGE_R: { p.x = xmax() * SQUARE_SIZE;  p.z = zmin() * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_B | REL_NGB_EDGE_R: { p.x = xmax() * SQUARE_SIZE;  p.z = zmax() * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_B | REL_NGB_EDGE_L: { p.x = xmin() * SQUARE_SIZE;  p.z = zmax() * SQUARE_SIZE; } break;
		// edges
		case REL_NGB_EDGE_T: { p.x = midx   * SQUARE_SIZE;  p.z = zmin() * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_R: { p.x = xmax() * SQUARE_SIZE;  p.z = midz   * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_B: { p.x = midx   * SQUARE_SIZE;  p.z = zmax() * SQUARE_SIZE; } break;
		case REL_NGB_EDGE_L: { p.x = xmin() * SQUARE_SIZE;  p.z = midz   * SQUARE_SIZE; } break;

		case 0: {
			// <ngb> had better be an actual neighbor
			assert(false);
		} break;
	}

	return p;
}

// clip an OVERLAPPING rectangle against our boundaries
//
// NOTE: the rectangle is only ASSUMED to not lie completely
// inside <this> (in which case this function acts as no-op),
// we do not explicitly test
//
SRectangle QTPFS::INode::ClipRectangle(const SRectangle& r) const {
	assert(GetRectangleRelation(r) != REL_RECT_EXTERIOR_NODE);

	SRectangle cr = SRectangle(0, 0, 0, 0);
	cr.x1 = std::max(int(xmin()), r.x1);
	cr.z1 = std::max(int(zmin()), r.z1);
	cr.x2 = std::min(int(xmax()), r.x2);
	cr.z2 = std::min(int(zmax()), r.z2);
	return cr;
}






QTPFS::QTNode::QTNode(
	unsigned int id,
	unsigned int x1, unsigned int z1,
	unsigned int x2, unsigned int z2)
{
	identifier = id;
	tempNum = -1;

	_xmin = x1;
	_zmin = z1;
	_xmax = x2;
	_zmax = z2;
	_xmid = (_xmin + _xmax) >> 1;
	_zmid = (_zmin + _zmax) >> 1;
	_xsize = _xmax - _xmin;
	_zsize = _zmax - _zmin;

	speedModAvg = 0.0f;
	speedModSum = 0.0f;
	moveCostAvg = 0.0f;
	
	// for leafs, all children remain NULL
	children.resize(QTNode::CHILD_COUNT, NULL);
}

QTPFS::QTNode::~QTNode() {
	children.clear();
	neighbors.clear();
}

void QTPFS::QTNode::Delete() {
	if (!IsLeaf()) {
		for (unsigned int i = 0; i < QTNode::CHILD_COUNT; i++) {
			children[i]->Delete(); children[i] = NULL;
		}
	}

	delete this;
}



bool QTPFS::QTNode::IsLeaf() const {
	assert(children.size() == QTNode::CHILD_COUNT);
	assert(
		(children[0] == NULL && children[1] == NULL && children[2] == NULL && children[3] == NULL) ||
		(children[0] != NULL && children[1] != NULL && children[2] != NULL && children[3] != NULL)
	);
	return (children[0] == NULL);
}



bool QTPFS::QTNode::Split(NodeLayer& nl) {
	if (xsize() <= 1 || zsize() <= 1) {
		return false;
	}

	// can only split leaf-nodes (ie. nodes with NULL-children)
	assert(children[NODE_IDX_TL] == NULL);
	assert(children[NODE_IDX_TR] == NULL);
	assert(children[NODE_IDX_BR] == NULL);
	assert(children[NODE_IDX_BL] == NULL);

	children[NODE_IDX_TL] = new QTNode(GetChildID(NODE_IDX_TL),  xmin(), zmin(),  xmid(), zmid());
	children[NODE_IDX_TR] = new QTNode(GetChildID(NODE_IDX_TR),  xmid(), zmin(),  xmax(), zmid());
	children[NODE_IDX_BR] = new QTNode(GetChildID(NODE_IDX_BR),  xmid(), zmid(),  xmax(), zmax());
	children[NODE_IDX_BL] = new QTNode(GetChildID(NODE_IDX_BL),  xmin(), zmid(),  xmid(), zmax());

	nl.SetNumLeafNodes(nl.GetNumLeafNodes() + (4 - 1));
	assert(!IsLeaf());
	return true;
}

bool QTPFS::QTNode::Merge(NodeLayer& nl) {
	if (IsLeaf()) {
		return false;
	}

	// get rid of our children completely, but not of <this>!
	for (unsigned int i = 0; i < QTNode::CHILD_COUNT; i++) {
		children[i]->Delete(); children[i] = NULL;
	}

	nl.SetNumLeafNodes(nl.GetNumLeafNodes() - (4 - 1));
	assert(IsLeaf());
	return true;
}



// re-tesselate a tree from the deepest node <n>
// that contains rectangle <r> (<n> will be found
// from any higher node passed in, including root)
//
void QTPFS::QTNode::PreTesselate(NodeLayer& nl, const SRectangle& r) {
	const unsigned int rel = GetRectangleRelation(r);

	if (rel == REL_RECT_EXTERIOR_NODE) {
		return;
	}

	// use <r> if it is fully inside <this>, otherwise clip
	const SRectangle& cr = (rel != REL_RECT_INTERIOR_NODE)? ClipRectangle(r): r;

	const bool leaf = IsLeaf();
	const bool cont = (rel != REL_RECT_INTERIOR_NODE)?
		(((xsize() >> 1) > (cr.x2 - cr.x1)) &&
		 ((zsize() >> 1) > (cr.z2 - cr.z1))):
		true;

	if (leaf || !cont) {
		Tesselate(nl, cr);
		return;
	}

	// continue recursion while our children are still larger than the clipped rectangle
	//
	// NOTE: this is a trade-off between minimizing the number of leaf-nodes (achieved by
	// re-tesselating in its entirety the deepest node that fully contains <r>) and cost
	// of re-tesselating (which grows as the node-count decreases, kept under control by
	// breaking <r> up into pieces while descending further)
	//
	for (unsigned int i = 0; i < QTNode::CHILD_COUNT; i++) {
		children[i]->PreTesselate(nl, cr);
	}
}



void QTPFS::QTNode::Tesselate(NodeLayer& nl, const SRectangle& r) {
	unsigned int numNewBinSquares = 0; // number of squares that changed bin within <r> after deformation
	unsigned int numRefBinSquares = 0; // number of squares in <r> NOT equal to the new ref-bin at <x1, z1>

	const std::vector<int>& oldSpeedBins = nl.GetOldSpeedBins();
	const std::vector<int>& curSpeedBins = nl.GetCurSpeedBins();
	const std::vector<float>& oldSpeedMods = nl.GetOldSpeedMods();
	const std::vector<float>& curSpeedMods = nl.GetCurSpeedMods();

	for (unsigned int hmx = r.x1; hmx < r.x2; hmx++) {
		for (unsigned int hmz = r.z1; hmz < r.z2; hmz++) {
			const unsigned int sqrIdx = hmz * gs->mapx + hmx;

			const int oldSpeedBin = oldSpeedBins[sqrIdx];
			const int curSpeedBin = curSpeedBins[sqrIdx];
			const int refSpeedBin = curSpeedBins[r.z1 * gs->mapx + r.x1];

			numNewBinSquares += (curSpeedBin != oldSpeedBin)? 1: 0;
			numRefBinSquares += (curSpeedBin != refSpeedBin)? 1: 0;

			speedModSum -= oldSpeedMods[sqrIdx];
			speedModSum += curSpeedMods[sqrIdx];
		}
	}

	assert(
		((numNewBinSquares == (r.GetWidth() * r.GetHeight())) && (numRefBinSquares == 0)) ||
		((numNewBinSquares >                               0) && (numRefBinSquares >  0)) ||
		((numNewBinSquares ==                              0) && (numRefBinSquares == 0))
	);

	// (re-)calculate the average cost of this node
	speedModAvg = speedModSum / (xsize() * zsize());
	moveCostAvg = (speedModAvg <= 0.001f)? std::numeric_limits<float>::infinity(): (1.0f / speedModAvg);

	// we want to *keep* splitting so long as not ALL squares
	// within <r> share the SAME bin, OR we keep finding one
	// that SWITCHED bins after the terrain change (we already
	// know this is true for the entire rectangle or we would
	// not have reached PreTesselate)
	//
	// NOTE: during tree construction, numNewBinSquares is ALWAYS
	// non-0 for the entire map-rectangle (see NodeLayer::Update)
	//
	// NOTE: if <r> fully overlaps <this>, then splitting is *not*
	// technically required whenever numRefBinSquares is zero, ie.
	// when ALL squares in <r> changed bins in unison
	//
	// if true, we are at the bottom of the recursion
	bool registerNode = true;

	if (numNewBinSquares > 0 && numRefBinSquares > 0) {
		Merge(nl);

		if (Split(nl)) {
			registerNode = false;

			for (unsigned int i = 0; i < QTNode::CHILD_COUNT; i++) {
				children[i]->Tesselate(nl, children[i]->ClipRectangle(r));
			}
		}
	}

	if (registerNode) {
		nl.RegisterNode(this);
	}
}



// get the maximum number of neighbors this node
// can have, based on its position / size and the
// assumption that all neighbors are 1x1
//
// NOTE: this intentionally does not count corners
unsigned int QTPFS::QTNode::GetMaxNumNeighbors() const {
	unsigned int n = 0;

	if (xmin() > (           0)) { n += zsize(); } // count EDGE_L ngbs
	if (xmax() < (gs->mapx - 1)) { n += zsize(); } // count EDGE_R ngbs
	if (zmin() > (           0)) { n += xsize(); } // count EDGE_T ngbs
	if (zmax() < (gs->mapy - 1)) { n += xsize(); } // count EDGE_B ngbs

	return n;
}





void QTPFS::QTNode::Serialize(std::fstream& fStream, bool read) {
	// overwritten when de-serializing
	unsigned int numChildren = IsLeaf()? 0: QTNode::CHILD_COUNT;

	if (read) {
		fStream.read(reinterpret_cast<char*>(&identifier), sizeof(unsigned int));
		fStream.read(reinterpret_cast<char*>(&numChildren), sizeof(unsigned int));

		fStream.read(reinterpret_cast<char*>(&speedModAvg), sizeof(float));
		fStream.read(reinterpret_cast<char*>(&speedModSum), sizeof(float));
		fStream.read(reinterpret_cast<char*>(&moveCostAvg), sizeof(float));

		if (numChildren > 0) {
			// re-create child nodes
			assert(IsLeaf());
			Split(*PathManager::GetSerializingNodeLayer());
		} else {
			// node was a leaf in an earlier life, register it
			PathManager::GetSerializingNodeLayer()->RegisterNode(this);
		}
	} else {
		fStream.write(reinterpret_cast<const char*>(&identifier), sizeof(unsigned int));
		fStream.write(reinterpret_cast<const char*>(&numChildren), sizeof(unsigned int));

		fStream.write(reinterpret_cast<const char*>(&speedModAvg), sizeof(float));
		fStream.write(reinterpret_cast<const char*>(&speedModSum), sizeof(float));
		fStream.write(reinterpret_cast<const char*>(&moveCostAvg), sizeof(float));
	}

	for (unsigned int i = 0; i < numChildren; i++) {
		children[i]->Serialize(fStream, read);
	}
}

unsigned int QTPFS::QTNode::GetNeighbors(const std::vector<INode*>& nodes, std::vector<INode*>& ngbs) {
	assert(IsLeaf());

	if (tempNum != gs->frameNum) {
		tempNum = gs->frameNum;

		// regenerate our neighbor cache for this frame
		if (GetMaxNumNeighbors() > 0) {
			neighbors.clear();
			neighbors.reserve(GetMaxNumNeighbors() + 4);

			bool ngbsL = false;
			bool ngbsR = false;
			bool ngbsT = false;
			bool ngbsB = false;

			INode* ngb = NULL;

			if (xmin() > 0) {
				const unsigned int hmx = xmin() - 1;

				// walk along EDGE_L (west) neighbors
				for (unsigned int hmz = zmin(); hmz < zmax(); ) {
					ngb = nodes[hmz * gs->mapx + hmx];
					hmz += ngb->zsize();
					neighbors.push_back((QTNode*) ngb);
				}

				ngbsL = true;
			}
			if (xmax() < static_cast<unsigned int>(gs->mapx)) {
				const unsigned int hmx = xmax() + 0;

				// walk along EDGE_R (east) neighbors
				for (unsigned int hmz = zmin(); hmz < zmax(); ) {
					ngb = nodes[hmz * gs->mapx + hmx];
					hmz += ngb->zsize();
					neighbors.push_back((QTNode*) ngb);
				}

				ngbsR = true;
			}

			if (zmin() > 0) {
				const unsigned int hmz = zmin() - 1;

				// walk along EDGE_T (north) neighbors
				for (unsigned int hmx = xmin(); hmx < xmax(); ) {
					ngb = nodes[hmz * gs->mapx + hmx];
					hmx += ngb->xsize();
					neighbors.push_back((QTNode*) ngb);
				}

				ngbsT = true;
			}
			if (zmax() < static_cast<unsigned int>(gs->mapy)) {
				const unsigned int hmz = zmax() + 0;

				// walk along EDGE_B (south) neighbors
				for (unsigned int hmx = xmin(); hmx < xmax(); ) {
					ngb = nodes[hmz * gs->mapx + hmx];
					hmx += ngb->xsize();
					neighbors.push_back((QTNode*) ngb);
				}

				ngbsB = true;
			}

			if (ngbsL && ngbsT) { neighbors.push_back((QTNode*) nodes[(zmin() - 1) * gs->mapx + (xmin() - 1)]); } // VERT_TL neighbor
			if (ngbsL && ngbsB) { neighbors.push_back((QTNode*) nodes[(zmax() + 0) * gs->mapx + (xmin() - 1)]); } // VERT_BL neighbor
			if (ngbsR && ngbsT) { neighbors.push_back((QTNode*) nodes[(zmin() - 1) * gs->mapx + (xmax() + 0)]); } // VERT_TR neighbor
			if (ngbsR && ngbsB) { neighbors.push_back((QTNode*) nodes[(zmax() + 0) * gs->mapx + (xmax() + 0)]); } // VERT_BR neighbor
		}
	}

	if (!neighbors.empty()) {
		ngbs.clear();
		ngbs.resize(neighbors.size());

		std::copy(neighbors.begin(), neighbors.end(), ngbs.begin());
	}

	return (neighbors.size());
}
