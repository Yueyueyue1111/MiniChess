#include <utility>
#include "state.hpp"
#include "minimax.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>

/*============================================================
 * Time management
 *
 * SearchContext::stop is checked every N nodes and set true
 * when the time budget is exhausted. Iterative deepening
 * in search() then returns the last fully-completed result.
 *============================================================*/
static constexpr int  TIME_CHECK_INTERVAL = 1024; // check every 1024 nodes
static constexpr double TIME_LIMIT_MS     = 4500.0; // leave 500ms safety margin
 
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
 
static TimePoint g_search_start;
 
static inline bool time_is_up(const SearchContext& ctx){
    if((ctx.nodes & (TIME_CHECK_INTERVAL - 1)) != 0) return false;
    double elapsed = std::chrono::duration<double, std::milli>(
        Clock::now() - g_search_start).count();
    return elapsed >= TIME_LIMIT_MS;
}


/*============================================================
 * Quiescence
 *============================================================*/
static const int Q_PIECE_VALUES[] = {0, 10, 50, 30, 30, 90, 900};

int quiescence(State *state, int alpha, int beta, int ply, SearchContext& ctx){
    ctx.nodes++;
    if(ctx.stop)return 0;

    // return the score for a winning terminal state
    if(state->game_state == WIN) return P_MAX - ply;
    if(state->game_state == DRAW) return 0;

    int stand_pat = state->evaluate(true, true, nullptr);
    if(stand_pat>=beta){
        return beta;
    }
    if(stand_pat>alpha){
        alpha=stand_pat;
    }

    if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
        state->get_legal_actions();
    }
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

        // 檢查目標格有沒有敵方棋子
        int captured_piece = state->piece_at(opponent_player, tr, tc);
        if (captured_piece > 0 && captured_piece <= 6) {
            int my_piece = state->piece_at(current_player, fr, fc);
            // MVV-LVA 排序分數
            int score = 10000 + (Q_PIECE_VALUES[captured_piece] * 10) - Q_PIECE_VALUES[my_piece];
            q_moves.push_back({move, score});
        }
    }

    std::sort(q_moves.begin(), q_moves.end(), [](const QMove& a, const QMove& b) {
        return a.score > b.score;
    });

    // 【核心 3】遍歷這些吃子步
    for (const auto& qm : q_moves) {
        State *next = state->next_state(qm.action);
        // Negamax 形式的遞迴
        int score = -quiescence(next, -beta, -alpha, ply + 1, ctx);
        delete next;

        if (score >= beta) {
            return beta; // Beta 剪枝
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

    /* === Negamax loop === */
    int best_score = M_MAX;
    bool is_first_move = true;

    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action

        State *next = (State*)state->next_state(action);
        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        // int score = same ? 
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //             -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        int score;
        if (is_first_move) {
            // 第一個走法：使用完整的 alpha-beta window 搜尋
            score = same ? 
                    eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
                   -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            is_first_move = false;
        } else {
            // 後續走法：先用 Null Window (alpha, alpha + 1) 進行測試
            score = same ?
                    eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, alpha + 1) :
                   -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);

            // 如果 Null Window 測試突破了 alpha，且還沒達到 beta 剪枝線
            // 代表這個走法可能比想像中更好，必須用完整的 window 重新搜尋 (Re-search)
            if (score > alpha && score < beta) {
                score = same ?
                        eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
                       -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            }
        }
        
        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.

        delete next;

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
    //throw std::runtime_error("AI is definitely being called!");
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

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
            // 第一個走法：完整 Window
            score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            is_first_move = false;
        } else {
            // 後續走法：Null Window 測試
            score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -alpha - 1, -alpha);

            // 測試失敗，觸發 Re-search
            if (score > alpha && score < beta) {
                score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
            }
        }
        // std::cout << "Action: " << action.first.first << " to " << action.second.first 
        //   << " | Score: " << score << std::endl; 
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



