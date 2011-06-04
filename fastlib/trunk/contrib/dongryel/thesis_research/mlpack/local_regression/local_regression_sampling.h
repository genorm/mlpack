/** @file local_regression_sampling.h
 *
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef MLPACK_LOCAL_REGRESSION_LOCAL_REGRESSION_SAMPLING_H
#define MLPACK_LOCAL_REGRESSION_LOCAL_REGRESSION_SAMPLING_H

#include <armadillo>
#include <deque>
#include <boost/scoped_array.hpp>
#include "core/monte_carlo/mean_variance_pair_matrix.h"

namespace mlpack {
namespace local_regression {
class LocalRegressionSampling {

  private:

    std::pair < core::monte_carlo::MeanVariancePairMatrix,
        core::monte_carlo::MeanVariancePairMatrix >
        avg_left_hand_side_for_reference_;

    std::pair < core::monte_carlo::MeanVariancePairVector,
        core::monte_carlo::MeanVariancePairVector >
        avg_right_hand_side_for_reference_;

    boost::scoped_array< core::monte_carlo::MeanVariancePairMatrix >
    *tmp_left_hand_sides_;

    boost::scoped_array< core::monte_carlo::MeanVariancePairVector >
    *tmp_right_hand_sides_;

    std::deque<bool> *converged_flags_;

  private:

    template<typename GlobalType>
    void push_back_reference_contribution_(
      const GlobalType &global,
      const arma::vec &random_variate,
      const arma::vec &random_rpoint,
      double random_rpoint_weight) {

      // Compute the dot product.
      double dot_product = arma::dot(
                             random_rpoint, random_variate);
      double cosine_value = cos(dot_product);
      double sine_value = sin(dot_product);

      // Accumulate the left hand and the righ hand sides.
      avg_left_hand_side_for_reference_.first.get(
        0, 0).push_back(cosine_value);
      avg_left_hand_side_for_reference_.second.get(
        0, 0).push_back(sine_value);
      avg_right_hand_side_for_reference_.first[0].push_back(
        random_rpoint_weight * cosine_value);
      avg_right_hand_side_for_reference_.second[0].push_back(
        random_rpoint_weight * sine_value);
      for(int j = 1; j < global.problem_dimension(); j++) {

        // Left hand sides.
        avg_left_hand_side_for_reference_.first.get(
          0, j).push_back(
            cosine_value *
            random_rpoint[j - 1]);
        avg_left_hand_side_for_reference_.second.get(
          0, j).push_back(
            sine_value *
            random_rpoint[j - 1]);
        avg_left_hand_side_for_reference_.first.get(
          j, 0).push_back(
            cosine_value *
            random_rpoint[j - 1]);
        avg_left_hand_side_for_reference_.second.get(
          j, 0).push_back(
            sine_value *
            random_rpoint[j - 1]);

        // Right hand sides.
        avg_right_hand_side_for_reference_.first[j].push_back(
          random_rpoint_weight * random_rpoint[j - 1] * cosine_value);
        avg_right_hand_side_for_reference_.second[j].push_back(
          random_rpoint_weight * random_rpoint[j - 1] * sine_value);
        for(int i = 1; i < global.problem_dimension(); i++) {
          avg_left_hand_side_for_reference_.first.get(
            i, j).push_back(
              cosine_value * random_rpoint[i - 1] * random_rpoint[j - 1]);
          avg_left_hand_side_for_reference_.second.get(
            i, j).push_back(
              sine_value * random_rpoint[i - 1] * random_rpoint[j - 1]);
        }
      }
    }

  public:

    template<typename GlobalType, typename TreeIteratorType>
    void Init(GlobalType &global, TreeIteratorType &qnode_it) {

      avg_left_hand_side_for_reference_.first.Init(
        global.problem_dimension(), global.problem_dimension());
      avg_left_hand_side_for_reference_.second.Init(
        global.problem_dimension(), global.problem_dimension());
      avg_right_hand_side_for_reference_.first.Init(
        global.problem_dimension());
      avg_right_hand_side_for_reference_.second.Init(
        global.problem_dimension());

      tmp_left_hand_sides_ = &(global.tmp_left_hand_sides());
      tmp_right_hand_sides_ = &(global.tmp_right_hand_sides());
      converged_flags_ = &(global.converged_flags());

      // First, initialize the query convergence slots.
      qnode_it.Reset();
      do {
        int qpoint_id;
        qnode_it.Next(&qpoint_id);
        (*converged_flags_)[qpoint_id] = false;
        (*tmp_left_hand_sides_)[qpoint_id].SetZero();
        (*tmp_right_hand_sides_)[qpoint_id].SetZero();
      }
      while(qnode_it.HasNext());
      qnode_it.Reset();
    }

    template<typename GlobalType, typename TreeIteratorType>
    void push_back_reference_contributions(
      GlobalType &global,
      const arma::vec &random_variate, TreeIteratorType &rnode_it) {

      // Randomly pick samples from the reference side.
      for(int r = 0; r < GlobalType::sampling_batch_size; r++) {

        // Random reference point.
        arma::vec random_rpoint;
        int random_rpoint_id;
        double random_rpoint_weight;
        rnode_it.RandomPick(
          &random_rpoint, &random_rpoint_id, &random_rpoint_weight);

        // Push back the contribution of the randomly chosen reference
        // point.
        this->push_back_reference_contribution_(
          global, random_variate, random_rpoint, random_rpoint_weight);

      } // end of looping over each reference sample.
    }

    template<typename GlobalType, typename TreeIteratorType>
    void AccumulateContributions(
      const GlobalType &global,
      const arma::vec &random_variate, TreeIteratorType &qnode_it) {

      qnode_it.Reset();
      do {

        // Query point and its ID and its projection.
        arma::vec qpoint;
        int qpoint_id;
        qnode_it.Next(&qpoint, &qpoint_id);
        double dot_product = arma::dot(qpoint, random_variate);

        // Multiply by 2 to offset the averaging of the cosine/sine
        // pair.
        double scaled_cosine_value = 2.0 * cos(dot_product);
        double scaled_sine_value = 2.0 * sin(dot_product);

        // Accumulate the contribution in this round.
        for(int j = 0; j < global.problem_dimension(); j++) {
          (*tmp_right_hand_sides_)[
            qpoint_id][j].ScaledCombineWith(
              scaled_cosine_value,
              avg_right_hand_side_for_reference_.first[j]);
          (*tmp_right_hand_sides_)[
            qpoint_id][j].ScaledCombineWith(
              scaled_sine_value,
              avg_right_hand_side_for_reference_.second[j]);

          for(int i = 0; i < global.problem_dimension(); i++) {
            (*tmp_left_hand_sides_)[
              qpoint_id].get(i, j).ScaledCombineWith(
                scaled_cosine_value,
                avg_left_hand_side_for_reference_.first.get(i, j));
            (*tmp_left_hand_sides_)[
              qpoint_id].get(i, j).ScaledCombineWith(
                scaled_sine_value,
                avg_left_hand_side_for_reference_.second.get(i, j));
          }
        }

      }
      while(qnode_it.HasNext());
    }

    template<typename TreeIteratorType>
    void Reset(const TreeIteratorType &rnode_it) {
      avg_left_hand_side_for_reference_.first.SetZero();
      avg_left_hand_side_for_reference_.second.SetZero();
      avg_right_hand_side_for_reference_.first.SetZero();
      avg_right_hand_side_for_reference_.second.SetZero();
      avg_left_hand_side_for_reference_.first.set_total_num_terms(
        rnode_it.count());
      avg_left_hand_side_for_reference_.second.set_total_num_terms(
        rnode_it.count());
      avg_right_hand_side_for_reference_.first.set_total_num_terms(
        rnode_it.count());
      avg_right_hand_side_for_reference_.second.set_total_num_terms(
        rnode_it.count());
    }
};
}
}

#endif
