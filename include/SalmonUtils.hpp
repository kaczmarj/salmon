#ifndef __SALMON_UTILS_HPP__
#define __SALMON_UTILS_HPP__

extern "C" {
#include "io_lib/scram.h"
#include "io_lib/os.h"
#undef min
#undef max
}

#include <algorithm>
#include <iostream>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <Eigen/Dense>

#include "spdlog/details/format.h"

#include "SalmonOpts.hpp"
#include "SalmonMath.hpp"

#include "LibraryFormat.hpp"
#include "ReadLibrary.hpp"
#include "TranscriptGeneMap.hpp"
#include "GenomicFeature.hpp"
#include "RapMapUtils.hpp"

class ReadExperiment;
class LibraryFormat;
class FragmentLengthDistribution;

namespace salmon{
namespace utils {

using std::string;
using NameVector = std::vector<string>;
using IndexVector = std::vector<size_t>;
using KmerVector = std::vector<uint64_t>;
using MateStatus = rapmap::utils::MateStatus;
    
// Keep track of the type of mapping that was obtained for this read
enum class MappingType : uint8_t { 
    UNMAPPED = 0, LEFT_ORPHAN = 1, RIGHT_ORPHAN = 2, BOTH_ORPHAN = 3, 
        PAIRED_MAPPED = 4,  SINGLE_MAPPED = 5 };

std::string str(const MappingType& mt);


// To keep track of short fragments (shorter than the k-mer length)
// on which the index was built.
struct ShortFragStats {
    size_t numTooShort{0};
    size_t shortest{std::numeric_limits<size_t>::max()};
};

// An enum class for direction to avoid potential errors
// with keeping everything as a bool
enum class Direction { FORWARD = 0, REVERSE_COMPLEMENT = 1, REVERSE = 2 };

// Returns FORWARD if isFwd is true and REVERSE_COMPLEMENT otherwise
constexpr inline Direction boolToDirection(bool isFwd) {
  return isFwd ? Direction::FORWARD : Direction::REVERSE_COMPLEMENT;
}

// Returns a uint64_t where the upper 32-bits
// contain tid and the lower 32-bits contain offset
uint64_t encode(uint64_t tid, uint64_t offset);

// Given a uin64_t generated by encode(), return the
// transcript id --- upper 32-bits
uint32_t transcript(uint64_t enc);

// Given a uin64_t generated by encode(), return the
// offset --- lower 32-bits
uint32_t offset(uint64_t enc);


LibraryFormat parseLibraryFormatStringNew(std::string& fmt);

std::vector<ReadLibrary> extractReadLibraries(boost::program_options::parsed_options& orderedOptions);

LibraryFormat parseLibraryFormatString(std::string& fmt);

size_t numberOfReadsInFastaFile(const std::string& fname);

bool readKmerOrder( const std::string& fname, std::vector<uint64_t>& kmers );

template <template<typename> class S, typename T>
bool overlap( const S<T> &a, const S<T> &b );

template< typename T >
TranscriptGeneMap transcriptToGeneMapFromFeatures( std::vector<GenomicFeature<T>> &feats );

TranscriptGeneMap transcriptGeneMapFromGTF(const std::string& fname, std::string key="gene_id");

TranscriptGeneMap readTranscriptToGeneMap( std::ifstream &ifile );

TranscriptGeneMap transcriptToGeneMapFromFasta( const std::string& transcriptsFile );

template <typename AbundanceVecT, typename ReadExpT>
Eigen::VectorXd updateEffectiveLengths(
        SalmonOpts& sopt,
        ReadExpT& readExp,
        Eigen::VectorXd& effLensIn,
        AbundanceVecT& alphas,
	bool finalRound = false);

/*
 * Use atomic compare-and-swap to update val to
 * val + inc (*in log-space*).  Update occurs in a loop in case other
 * threads update in the meantime.
 */
inline void incLoopLog(tbb::atomic<double>& val, double inc) {
	double oldMass = val.load();
	double returnedMass = oldMass;
	double newMass{salmon::math::LOG_0};
	do {
	  oldMass = returnedMass;
 	  newMass = salmon::math::logAdd(oldMass, inc);
	  returnedMass = val.compare_and_swap(newMass, oldMass);
	} while (returnedMass != oldMass);
}


/*
 * Same as above, but overloaded for "plain" doubles
 */
inline void incLoop(double& val, double inc) {
	val += inc;
}


/*
 * Use atomic compare-and-swap to update val to
 * val + inc.  Update occurs in a loop in case other
 * threads update in the meantime.
 */
inline void incLoop(tbb::atomic<double>& val, double inc) {
        double oldMass = val.load();
        double returnedMass = oldMass;
        double newMass{oldMass + inc};
        do {
            oldMass = returnedMass;
            newMass = oldMass + inc;
            returnedMass = val.compare_and_swap(newMass, oldMass);
        } while (returnedMass != oldMass);
}

bool processQuantOptions(SalmonOpts& sopt, boost::program_options::variables_map& vm, int32_t numBiasSamples);



void aggregateEstimatesToGeneLevel(TranscriptGeneMap& tgm, boost::filesystem::path& inputPath);

// NOTE: Throws an invalid_argument exception of the quant or quant_bias_corrected files do
// not exist!
void generateGeneLevelEstimates(boost::filesystem::path& geneMapPath,
                                boost::filesystem::path& estDir);

    enum class OrphanStatus: uint8_t { LeftOrphan = 0, RightOrphan = 1, Paired = 2 };

    bool headersAreConsistent(SAM_hdr* h1, SAM_hdr* h2);

    bool headersAreConsistent(std::vector<SAM_hdr*>&& headers);

    inline void reverseComplement(const char* s, int32_t l, std::string& o) {
        if (l > o.size()) { o.resize(l, 'A'); }
        int32_t j = 0;
        for (int32_t i = l-1; i >= 0; --i, ++j) {
            switch(s[i]) {
            case 'A':
            case 'a':
                o[j] = 'T';
                break;
            case 'C':
            case 'c':
                o[j] = 'G';
                break;
            case 'T':
            case 't':
                o[j] = 'A';
                break;
            case 'G':
            case 'g':
                o[j] = 'C';
                break;
            default:
                o[j] = 'N';
                break;
            } 
        }
    }

    inline std::string reverseComplement(const char* s, int32_t l) {
        std::string o(l, 'A');
        reverseComplement(s, l, o);
        return o;
    }
   
    template <typename AlnLibT>
    void writeAbundances(const SalmonOpts& sopt,
                         AlnLibT& alnLib,
                         boost::filesystem::path& fname,
                         std::string headerComments="");

    template <typename AlnLibT>
    void writeAbundancesFromCollapsed(const SalmonOpts& sopt,
                         AlnLibT& alnLib,
                         boost::filesystem::path& fname,
                         std::string headerComments="");

    template <typename AlnLibT>
    void normalizeAlphas(const SalmonOpts& sopt,
                         AlnLibT& alnLib);

    double logAlignFormatProb(const LibraryFormat observed,
                              const LibraryFormat expected,
                              int32_t start, bool isForward,
                              rapmap::utils::MateStatus ms,
                              double incompatPrior);

    bool compatibleHit(const LibraryFormat expected, int32_t start, bool isForward, MateStatus ms);
    bool compatibleHit(const LibraryFormat expected, const LibraryFormat observed);

    std::ostream& operator<<(std::ostream& os, OrphanStatus s);
    /**
    *  Given the information about the position and strand from which a paired-end
    *  read originated, return the library format with which it is compatible.
    */
    LibraryFormat hitType(int32_t end1Start, bool end1Fwd,
                          int32_t end2Start, bool end2Fwd);
    LibraryFormat hitType(int32_t end1Start, bool end1Fwd, uint32_t len1,
                          int32_t end2Start, bool end2Fwd, uint32_t len2, bool canDovetail);
    /**
    *  Given the information about the position and strand from which the
    *  single-end read originated, return the library format with which it
    *  is compatible.
    */
    LibraryFormat hitType(int32_t readStart, bool isForward);

}
}

#endif // __SALMON_UTILS_HPP__
