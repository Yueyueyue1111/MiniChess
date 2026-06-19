#include <utility>
#include "state.hpp"
#include "minimax.hpp"
<<<<<<< HEAD

=======
#include <iostream>
#include <algorithm>
#include <chrono>

enum TTFlag {
    TT_EXACT,       // 精確分數 (在 Alpha 與 Beta 之間)
    TT_LOWERBOUND,  // 分數下限 (發生 Beta 剪枝，真實分數可能更高)
    TT_UPPERBOUND   // 分數上限 (所有走法都低於 Alpha，真實分數可能更低)
};

struct TTEntry {
    uint64_t hash = 0;
    int depth = -1;
    int score = 0;
    TTFlag flag;
    Move best_move; // 把這個盤面最好的走法也存起來，這對排序超級有用！
};

// 宣告一個固定大小的 TT 表 (使用 2 的次方，方便用 bitwise AND 加速)
// 1 << 20 大約是 100 萬個 entry，佔用幾十 MB 記憶體，很適合
const int TT_SIZE = 1 << 20;
static std::vector<TTEntry> TT(TT_SIZE);

/*============================================================
 * Mid-search time check
 *
 * ubgi.cpp sets ctx.stop from an external timer, but only
 * checks it AFTER search() returns -- so a slow depth could
 * run way over budget.
 *
 * Solution: every 1024 nodes, check the clock ourselves and
 * set ctx.stop if we are over the limit. ubgi.cpp then sees
 * ctx.stop, discards the incomplete depth, and returns the
 * last good result.
 *
 * g_move_time_ms is set at the start of each search() call.
 * We use 90% of the budget to leave a safety margin.
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
    check_time(ctx);
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
        // State *next = state->next_state(qm.action);
        // // Negamax 形式的遞迴
        // int score = -quiescence(next, -beta, -alpha, ply + 1, ctx);
        // delete next;
        State next_state = *state;
        next_state.apply_move(qm.action);
        
        // 傳遞記憶體位址進行遞迴
        int score = -quiescence(&next_state, -beta, -alpha, ply + 1, ctx);

        if (score >= beta) {
            return beta; // Beta 剪枝
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f

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
<<<<<<< HEAD
    const MMParams& p
=======
    const MMParams& p,
    int alpha,
    int beta
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
<<<<<<< HEAD
=======
    check_time(ctx);
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
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
<<<<<<< HEAD
    if (state->game_state == WIN) return P_MAX + ply;
    //if (state->game_state == NONE) return M_MAX - ply;

=======
    if(state->game_state == WIN){
        return P_MAX - ply; 
    }
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

<<<<<<< HEAD
    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        ); 
        history.pop(state->hash());
        return score;
    }

    /* === Negamax loop === */
    int best_score = M_MAX;

    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action
        State *next = state->next_state(action);
        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        int score = -eval_ctx(next, depth - 1, history, ply + 1, ctx, p);
        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.

        delete next;

        // [ Hackathon TODO 3-5 ]
        // update best_score if this child is better.
        if (score > best_score) {
            best_score = score;
        }
        
    }

=======
    int original_alpha = alpha; 
    
    int tt_index = state->hash() & (TT_SIZE - 1); // 等同於 hash % TT_SIZE，但更快
    TTEntry& tte = TT[tt_index];
    Move tt_best_move; // 稍後可以用來做最佳步排序

    if (tte.hash == state->hash()) {
        tt_best_move = tte.best_move; // 就算深度不夠，這步通常也是最好的，存下來做 ordering

        // 如果快取的深度 >= 我們現在要求的深度，就可以考慮直接拿來用
        if (tte.depth >= depth) {
            if (tte.flag == TT_EXACT) {
                history.pop(state->hash());
                return tte.score;
            } else if (tte.flag == TT_LOWERBOUND && tte.score >= beta) {
                history.pop(state->hash());
                return tte.score;
            } else if (tte.flag == TT_UPPERBOUND && tte.score <= alpha) {
                history.pop(state->hash());
                return tte.score;
            }
        }
    }

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

    // 進行走法排序 (Move Ordering)
    // std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
    //     int cap_a = state->piece_at(opp_player, a.second.first, a.second.second);
    //     int cap_b = state->piece_at(opp_player, b.second.first, b.second.second);
        
    //     // 簡單排序策略：有吃子的走法排在前面，且吃的子價值越高越優先
    //     // (如果想做得更好，可以把 Q_PIECE_VALUES 拿來這裡用 MVV-LVA)
    //     return cap_a > cap_b; 
    // });
    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
        // 1. TT 裡的最佳步絕對優先！
        if (a == b) return false;
        if (a == tt_best_move) return true;
        if (b == tt_best_move) return false;

        // 2. 吃子優先 (使用你先前的邏輯)
        int cap_a = state->piece_at(opp_player, a.second.first, a.second.second);
        int cap_b = state->piece_at(opp_player, b.second.first, b.second.second);
        
        return cap_a > cap_b; 
    });

    /* === Negamax loop === */
    int best_score = M_MAX;
    bool is_first_move = true;
    Move best_action;

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
        //     // 第一個走法：使用完整的 alpha-beta window 搜尋
        //     score = same ? 
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //            -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        //     is_first_move = false;
        // } else {
        //     // 後續走法：先用 Null Window (alpha, alpha + 1) 進行測試
        //     score = same ?
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, alpha + 1) :
        //            -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);

        //     // 如果 Null Window 測試突破了 alpha，且還沒達到 beta 剪枝線
        //     // 代表這個走法可能比想像中更好，必須用完整的 window 重新搜尋 (Re-search)
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
            best_action = action;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            //history.pop(state->hash());
            break;
        }

    }

    if (ctx.stop) {
            history.pop(state->hash());
            return best_score;
        }

    /* === 寫入 置換表 (Transposition Table) === */
    TTFlag flag;
    if (best_score <= original_alpha) {
        flag = TT_UPPERBOUND; // 所有走法都很爛，連原本的 alpha 都沒突破
    } else if (best_score >= beta) {
        flag = TT_LOWERBOUND; // 發生了剪枝，這個盤面至少有 best_score 這麼好
    } else {
        flag = TT_EXACT;      // 找到了真正的最佳分數
    }

    // 替換掉舊的 entry (這裡使用最簡單的 Always Replace 策略)
    tte.hash = state->hash();
    tte.depth = depth;
    tte.score = best_score;
    tte.flag = flag;
    tte.best_move = best_action;
    // tte.best_move 最好在你迴圈裡找到 best_score 時順便記錄下來
    // 你可以在 for 迴圈外面宣告一個 Move best_action;
    // 在 if (score > best_score) 裡面加上 best_action = action;
    // 最後在這裡 tte.best_move = best_action;

>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
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
<<<<<<< HEAD
    ctx.reset();
=======
    //throw std::runtime_error("AI is definitely being called!");
    ctx.reset();
    ctx.stop = false;
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

<<<<<<< HEAD

    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        State *next = state->next_state(action);
        int score = -eval_ctx(next, depth-1, history, 1, ctx, p);
        delete next;
        if(score > best_score){
            // keep this move if it is the best so far
            best_score=score;
            result.best_move=action;
            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }  
=======
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

    // Fallback: always have a valid move to return even if time expires instantly
    if(!state->legal_actions.empty()){
        result.best_move = state->legal_actions[0];
    }
 
    // Start clock for mid-search time checks
    if(depth==1)g_search_start = Clock::now();
    g_move_time_ms = 1800; // 90% of 2000ms; change if your time control differs

    int best_score = M_MAX + 200;
    int move_index = 0;
    //int total_moves = (int)state->legal_actions.size();


    auto actions = state->legal_actions;
    int total_moves = (int)actions.size();
    int opp_player = 1 - state->player;
    
    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
        int cap_a = state->piece_at(opp_player, a.second.first, a.second.second);
        int cap_b = state->piece_at(opp_player, b.second.first, b.second.second);
        return cap_a > cap_b; 
    });
    //initialize alpha-beta
    int alpha = M_MAX;
    int beta = P_MAX;
    bool is_first_move = true;


    for(auto& action : actions){
        if (ctx.stop) break;
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        State next_state = *state;
        next_state.apply_move(action);
        //int score = -eval_ctx(next, depth-1, history, 1, ctx, p, -beta, -alpha);
        // int score;
        // if (is_first_move) {
        //     // 第一個走法：完整 Window
        //     score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
        //     is_first_move = false;
        // } else {
        //     // 後續走法：Null Window 測試
        //     score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -alpha - 1, -alpha);

        //     // 測試失敗，觸發 Re-search
        //     if (score > alpha && score < beta) {
        //         score = -eval_ctx(next, depth - 1, history, 1, ctx, p, -beta, -alpha);
        //     }
        // }
        // // std::cout << "Action: " << action.first.first << " to " << action.second.first 
        // //   << " | Score: " << score << std::endl; 
        // delete next;
        int score;
        if (is_first_move) {
            score = -eval_ctx(&next_state, depth - 1, history, 1, ctx, p, -beta, -alpha);
            is_first_move = false;
        } else {
            score = -eval_ctx(&next_state, depth - 1, history, 1, ctx, p, -alpha - 1, -alpha);

            if (score > alpha && score < beta) {
                score = -eval_ctx(&next_state, depth - 1, history, 1, ctx, p, -beta, -alpha);
            }
        }

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
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
        move_index++;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
<<<<<<< HEAD
    result.score = best_score;
    return result;
=======
        result.score = best_score;

        return result;
>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
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
<<<<<<< HEAD
=======



>>>>>>> dc23599caf428c598707c4740a32d8f63e198c1f
