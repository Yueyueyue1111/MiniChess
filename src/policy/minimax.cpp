#include <utility>
#include "state.hpp"
#include "minimax.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

/*============================================================
 * Mid-search time check
 *============================================================*/
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
 
static TimePoint g_search_start;
static int64_t   g_move_time_ms = 0;  // 0 = no limit (depth-only mode)
 
static inline void check_time(SearchContext& ctx){
    if((ctx.nodes & 1023) != 0) return;  // only check every 1024 nodes
    if(g_move_time_ms <= 0) return;       // no time limit set
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - g_search_start).count();
    if(elapsed >= g_move_time_ms * 9 / 10)
        ctx.stop = true;
}

/*============================================================
 * Quiescence
 *============================================================*/
static const int Q_PIECE_VALUES[] = {0, 10, 50, 30, 30, 90, 900};

int quiescence(State *state, int alpha, int beta, int ply, SearchContext& ctx){
    ctx.nodes++;
    //check_time(ctx);
    if(ctx.stop)return 0;

    // return the score for a winning terminal state
    if(state->game_state == WIN) return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
        state->get_legal_actions();
    }
    int stand_pat = state->evaluate(true, true, nullptr);
    if(stand_pat>=beta){
        return beta;
    }
    if(stand_pat>alpha){
        alpha=stand_pat;
    }

    // if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
    //     state->get_legal_actions();
    // }
    auto actions = state->legal_actions;
    if (actions.empty()) return stand_pat;

    struct QMove {
        Move action;
        int score;
    };
    std::vector<QMove> q_moves;
    q_moves.reserve(actions.size());

    int current_player = state->player;
    int opponent_player = 1 - current_player;

    for (const auto& move : actions) {
        int tr = move.second.first;
        int tc = move.second.second;
        int fr = move.first.first;
        int fc = move.first.second;

        int captured_piece = state->piece_at(opponent_player, tr, tc);
        if (captured_piece > 0 && captured_piece <= 6) {
            int my_piece = state->piece_at(current_player, fr, fc);
            int score = 10000 + (Q_PIECE_VALUES[captured_piece] * 10) - Q_PIECE_VALUES[my_piece];
            q_moves.push_back({move, score});
        }
    }

    std::sort(q_moves.begin(), q_moves.end(), [](const QMove& a, const QMove& b) {
        return a.score > b.score;
    });

    for (const auto& qm : q_moves) {
        State *next = state->next_state(qm.action);
        int score = -quiescence(next, -beta, -alpha, ply + 1, ctx);
        delete next;

        if (score >= beta) {
            return beta; 
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

/*============================================================
 * MiniMax ??? eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int MiniMax::eval_ctx(
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p,
    int alpha,
    int beta
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    //check_time(ctx);
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */

    // [ Hackathon TODO 3-1 ]
    // return the score for a winning terminal state
    // Hint: prefer faster wins by using ply.
    if(state->game_state == WIN){
        return P_MAX - ply; 
    }
    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    if(depth <= 0){
        // int score = state->evaluate(
        //     p.use_kp_eval, p.use_eval_mobility, &history
        // ); 
        // history.pop(state->hash());
        // return score;
        int val = quiescence(state, alpha, beta, ply, ctx);
        history.pop(state->hash());
        return val;
    }


    auto actions = state->legal_actions;
    int opp_player = 1 - state->player;

    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
        int cap_a = state->piece_at(opp_player, a.second.first, a.second.second);
        int cap_b = state->piece_at(opp_player, b.second.first, b.second.second);
        
        
        return cap_a > cap_b; 
    });
    /* === Negamax loop === */
    int best_score = M_MAX;
    bool is_first_move = true;

    for(auto& action : actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action

        // State *next = (State*)state->next_state(action);
        // bool same = next->same_player_as_parent();
        State next_state = *state;  
        next_state.apply_move(action);
        // [Hackathon TODO 3-3]
        // search the child one level deeper
        // int score = same ? 
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //             -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        
        int score;
        if (is_first_move) {
            score = -eval_ctx(&next_state, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            is_first_move = false;
        } else {
            score = -eval_ctx(&next_state, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);
            if (score > alpha && score < beta) {
                score = -eval_ctx(&next_state, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            }
        }
        
        // int score;
        // if (is_first_move) {
        //     score = same ? 
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //            -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        //     is_first_move = false;
        // } else {
        //     score = same ?
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, alpha + 1) :
        //            -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);

        //     if (score > alpha && score < beta) {
        //         score = same ?
        //                 eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //                -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        //     }
        // }
        
        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.

        //delete next;

        // [ Hackathon TODO 3-5 ]
        // update best_score if this child is better.
        // if(score>best_score){
        //     best_score=score;
        // }
        if(score > best_score){
            best_score = score;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            history.pop(state->hash());
            break;
        }

    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * MiniMax ??? search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;
    result.score=0;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    // if (ctx.nodes == 0 && state->legal_actions.size() > 10) {
    //     for (const auto& action : state->legal_actions) {
    //         if (action.first.first == 5 && action.first.second == 1 &&
    //             action.second.first == 3 && action.second.second == 2) {
    //             result.best_move = action;
    //             result.score = 0;
    //             return result; 
    //         }
    //     }
    // }

    // Fallback
    if (!state->legal_actions.empty()) {
        result.best_move = state->legal_actions[0]; 
    } else {
        result.best_move = {{0,0}, {0,0}};
    }
 
    // Start clock for mid-search time checks
    // g_search_start = Clock::now();
    // g_move_time_ms = 1800;
    int best_score = M_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    //initialize alpha-beta
    int alpha = M_MAX;
    int beta = P_MAX;
    bool is_first_move = true;


    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        State *next = state->next_state(action);
        //int score = -eval_ctx(next, depth-1, history, 1, ctx, p, -beta, -alpha);
        int score;
        if (is_first_move) {
            score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            is_first_move = false;
        } else {
            score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -alpha - 1, -alpha);

            if (score > alpha && score < beta) {
                score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            }
        }
        delete next;
        if(score > best_score){
            // [ Hackathon TODO 4-2 ]
            // keep this move if it is the best so far
            best_score=score;
            result.best_move=action;

            if(best_score>alpha){
                alpha=best_score;
            }

            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }
        move_index++;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
        result.score = best_score;

        return result;
} 


/*============================================================
 * MiniMax ??? default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}



