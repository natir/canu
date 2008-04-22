
/**************************************************************************
 * This file is part of Celera Assembler, a software program that 
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, The Venter Institute. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received (LICENSE.txt) a copy of the GNU General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/

#ifndef INCLUDE_AS_BOG_UNITIGGRAPH
#define INCLUDE_AS_BOG_UNITIGGRAPH

#include <set>
#include <iostream>
#include "AS_BOG_Datatypes.hh"
#include "AS_BOG_ChunkGraph.hh"

namespace AS_BOG{

    typedef std::vector<iuid>                 FragmentList;
    typedef std::map<iuid, FragmentList>      FragmentEdgeList;

    typedef IntMultiPos                       DoveTailNode;
    typedef std::vector<DoveTailNode>         DoveTailPath;

    typedef DoveTailPath::iterator            DoveTailIter;
    typedef DoveTailPath::const_iterator      DoveTailConstIter;

    typedef std::vector<iuid>                 ContaineeList;	
    typedef std::map<iuid, ContaineeList>     ContainerMap;

    typedef std::map<iuid, SeqInterval>       FragmentPositionMap;

    inline bool isReverse( SeqInterval pos ) {
        return(pos.bgn > pos.end);
    }

    struct UnitigBreakPoint {
        int         fragNumber;       // the number of the fragment in the unitig

        SeqInterval position;         // coordinates in unitig
        FragmentEnd fragEnd;          // frag id and which end to break on 

        iuid        inUnitig;         // iid of the incoming unitig
        int         inSize;           // the size of the incoming unitig
        int         inFrags;          // the number of fragments in incoming unitig

        UnitigBreakPoint(iuid id=0, fragment_end_type end=FIVE_PRIME) {
            fragNumber   = 0;
            position.bgn = 0;
            position.end = 0;
            fragEnd      = FragmentEnd(id, end);
            inUnitig     = 0;
            inSize       = 0;
            inFrags      = 0;
        }
    };

    //  Unfortunately, this does need to be a list.  Search for pop_front.
    //
    typedef std::list<UnitigBreakPoint> UnitigBreakPoints;


    struct Unitig{

        Unitig(bool report=false);
        ~Unitig(void);		

        // Sort frags by position on the unitig
        void sort();

        // Compute unitig based on given dovetails and containments
        void recomputeFragmentPositions(ContainerMap *,BestContainmentMap*, BestOverlapGraph*);
        void computeFragmentPositions(BestOverlapGraph*);

        void shiftCoordinates(int);
        void reverseComplement();
        void reverseComplement(int offset, BestOverlapGraph *);

        // Accessor methods
        float getAvgRho(void);
        static void setGlobalArrivalRate(float global_arrival_rate);
        void setLocalArrivalRate(float local_arrival_rate);
        float getLocalArrivalRate(void);
        float getCovStat(void);
        long getLength(void);
        long getNumFrags(void);
        long getSumFragLength(void);
        long getNumRandomFrags(void); // For now, same as numFrags, but should be randomly sampled frag count
        DoveTailNode getLastBackboneNode(iuid&);

        iuid         id(void) { return(_id); };

        void addFrag(DoveTailNode, int offset=0, bool report=false);

        static iuid fragIn(iuid fragId) {
            if (_inUnitig == NULL)
                return 0;
            return _inUnitig[fragId];
        };

        static void resetFragUnitigMap(iuid numFrags) {
            if (_inUnitig == NULL)
                _inUnitig = new iuid[numFrags+1];
            memset(_inUnitig, 0, (numFrags+1) * sizeof(iuid));
        };

        // Public Member Variables
        DoveTailPath *dovetail_path_ptr;

    private:
        void placeContains( const ContainerMap *, BestContainmentMap*,
                            const iuid , const SeqInterval, const int level );

        // Do not access these private variables directly, they may
        // not be computed yet, use accessors!
        //
        float  _avgRho;
        float  _covStat;
        long   _length;
        long   _numFrags;
        long   _numRandomFrags;
        float  _localArrivalRate;
        iuid   _id;

        static iuid   nextId;
        static float _globalArrivalRate;
        static iuid *_inUnitig;

    };


    struct BestEdgeCounts{
        int oneWayBest;
        int dovetail;
        int neither;
        int contained;

        BestEdgeCounts() : oneWayBest(0), dovetail(0), neither(0), contained(0) {}
        BestEdgeCounts operator+=(BestEdgeCounts other){
            oneWayBest += other.oneWayBest;
            dovetail   += other.dovetail;
            neither    += other.neither;
            contained  += other.contained;
        } 
    };
    typedef std::vector<Unitig*> UnitigVector;
    typedef UnitigVector::iterator UnitigsIter;
    typedef UnitigVector::const_iterator UnitigsConstIter;
    typedef UnitigVector::reverse_iterator UnitigsRIter;

    struct UnitigGraph{
        // This will store the entire set of unitigs that are generated
        // It's just a unitig container.
        UnitigGraph( BestOverlapGraph*);
        ~UnitigGraph();

        // Call this on a chunk graph pointer to build a unitig graph
        void build(ChunkGraph *cg_ptr);

        // For compatibility with the rest of the assembler
        void writeIUMtoFile(char *filename, int fragment_count_target);

        // For compatibility with the rest of the assembler
        void readIUMsFromFile(const char *filename, iuid maxIID);

        float getGlobalArrivalRate(long total_random_frags_in_genome=0, long genome_size=0);

        void breakUnitigs();
        void printUnitigBreaks();

        void filterBreakPoints( Unitig *, UnitigBreakPoints &);

        UnitigBreakPoint selectSmall(const Unitig *tig,
                                     const UnitigBreakPoints &smalls,
                                     const UnitigBreakPoint &big,
                                     int   &lastBPCoord,
                                     int   &lastBPFragNum);

        UnitigVector* breakUnitigAt( Unitig *, UnitigBreakPoints &);

        // Counts status of best edges internal to a unitig
        BestEdgeCounts countInternalBestEdges( const Unitig *);
        BestEdgeCounts countInternalBestEdges( ); // all unitigs

        // Unitigs are the dove tails and their contained fragments
        UnitigVector *unitigs;

        BestOverlapGraph *bog_ptr;

    private:
        // Given a fragment, it will follow it's overlaps until 
        //   the end, and add them to the unitig
        void populateUnitig(
                            Unitig* unitig, 
                            iuid src_frag_id, 
                            fragment_end_type whichEnd,
                            ChunkGraph *cg_ptr,
                            int offset);

        // Inverts the containment map to key by container, instead of containee
        void _build_container_map(BestContainmentMap*);

        // Build containee list
        ContainerMap *_extract_containees(DoveTailPath *dtp_ptr, 
                                          ContainerMap *cntnrmap_ptr);
        // Merge Unitigs by follow the bid's
        void mergeAllUnitigs( std::map<iuid,iuid> *);
        void mergeUnitigs(Unitig*, std::set<iuid> *, std::map<iuid,iuid> *);

        // handles next iteration setup for mergUnitigs
        BestEdgeOverlap* nextJoiner( Unitig*,
                                     iuid &aPrev, iuid &fragA, int &tigEnd, bool &begRev,
                                     BestEdgeOverlap *&fivePrime, BestEdgeOverlap *&threePrime );

        // Compute the global arrival rate based on the unitig rho's.
        float _compute_global_arrival_rate(void);

        FragmentEdgeList  unitigIntersect;
        ContainerMap     *cntnrmap_ptr;
    };
		
};

#endif

