#include <utility>
#include "state.hpp"
#include "minimax.hpp"
#include <iostream>
#include <algorithm>


//Quiesce Search
int MiniMax::quiesce(State *state, int alpha, int beta, GameHistory& history, SearchContext& ctx, const MMParams& p) {
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    state->get_legal_actions();
    for (auto& action : state->legal_actions) {
        // 重要：這裡要過濾，只搜尋「吃子」的動作 (Capture)
        // 假設您的 state 類別中有方法可以判斷是否為吃子，或者透過檢查 board
        if (!state->is_capture(action)) continue; 

        State *next = state->next_state(action);
        int score = -quiesce(next, -beta, -alpha, history, ctx, p);
        delete next;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
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
        return quiesce(state, alpha, beta, history, ctx, p);
    }

    if(state->legal_actions.empty()){
        if(state->game_state == WIN) return P_MAX - ply;
        if(state->game_state == DRAW) return 0;
        return -P_MAX + ply; // 這裡強制回傳輸棋分數，AI 才會想辦法避免走入此路
    }

    /* === Negamax loop === */
    int best_score = M_MAX;
    bool is_first_move = true;

    std::vector<Move> actions = state->legal_actions;
    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
        bool a_cap = state->is_capture(a);
        bool b_cap = state->is_capture(b);
        
        if (a_cap && b_cap) {
            // 如果都是吃子，比較價值 (假設您有獲取棋子價值的函數)
            // 例如：被吃的棋子價值越高，越優先
            return state->piece_at(1-state->player, a.second.first, a.second.second) > state->piece_at(1-state->player, b.second.first, b.second.second);
        }
        
        // 如果只有一個吃子，吃子的優先
        if (a_cap != b_cap) return a_cap > b_cap;
        
        return false; 
    });

    for(auto& action : actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action

        State *next = (State*)state->next_state(action);
        //bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        
        // int score = same ? 
        //             eval_ctx(next, depth, history, ply + 1, ctx, p, alpha, beta) :
        //             -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        int score;
        if (is_first_move) {
            // 第一個著法：用正常的 Alpha-Beta 搜尋
            score = -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
        } else {
            // 後續著法：使用「極窄視窗」測試
            score = -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -alpha - 1, -alpha);
            
            // 如果測試結果「打臉」了我們的假設 (score > alpha)，表示此著法可能更好，需重搜
            if (score > alpha && score < beta) {
                score = -eval_ctx(next, depth - 1, history, ply + 1, ctx, p, -beta, -alpha);
            }
        }

        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.
        is_first_move = false;
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

    if (state->legal_actions.empty()) {
        result.score = -P_MAX; // 或視情況給予平局分數
        return result;
    }

    int best_score = 2*M_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    result.best_move = state->legal_actions.empty() ? Move() : state->legal_actions[0];

    //initialize alpha-beta
    int alpha = 2*M_MAX;
    int beta = 2*P_MAX;

    std::vector<Move> actions = state->legal_actions;
    std::sort(actions.begin(), actions.end(), [&](const Move& a, const Move& b) {
        bool a_cap = state->is_capture(a);
        bool b_cap = state->is_capture(b);
        
        if (a_cap && b_cap) {
            // 如果都是吃子，比較價值 (假設您有獲取棋子價值的函數)
            // 例如：被吃的棋子價值越高，越優先
            return state->piece_at(1-state->player, a.second.first, a.second.second) > state->piece_at(1-state->player, b.second.first, b.second.second);
        }
        
        // 如果只有一個吃子，吃子的優先
        if (a_cap != b_cap) return a_cap > b_cap;
        
        return false; 
    });

    for(auto& action : actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        State *next = state->next_state(action);
        int score = -eval_ctx(next, depth-1, history, 1, ctx, p, -beta, -alpha);
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



