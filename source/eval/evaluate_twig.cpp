﻿#include "../shogi.h"

#ifdef EVAL_TWIG

#include <fstream>
#include <iostream>

#include "../evaluate.h"
#include "../position.h"

using namespace std;

// eval cacheを用いるか。
//#define USE_EHASH

namespace Eval
{

// KKPファイル名
#define KKP_BIN "KKP_synthesized.bin"//"eval\\KKP_synthesized.bin"

// KPPファイル名
#define KPP_BIN "KPP_synthesized.bin"//"eval\\KPP_synthesized.bin"

// KKファイル名
#define KK_BIN "KK_synthesized.bin"//"eval\\KK_synthesized.bin"

  typedef std::array<int16_t, 2> ValueKpp;
  typedef std::array<int32_t, 2> ValueKkp;
  typedef std::array<int32_t, 2> ValueKk;

  // KPP
  ValueKpp kpp[SQ_NB][fe_end][fe_end];

  // KKP
  ValueKkp kkp[SQ_NB][SQ_NB][fe_end];

  // KK
  ValueKk kk[SQ_NB][SQ_NB];

  // 評価関数ファイルを読み込む
  void load_eval()
  {
#if 1
    do {
      // KK
      std::ifstream ifsKK(KK_BIN, std::ios::binary);
      if (ifsKK) ifsKK.read(reinterpret_cast<char*>(kk), sizeof(kk));
      else goto Error;

      // KKP
      std::ifstream ifsKKP(KKP_BIN, std::ios::binary);
      if (ifsKKP) ifsKKP.read(reinterpret_cast<char*>(kkp), sizeof(kkp));
      else goto Error;

      // KPP
      std::ifstream ifsKPP(KPP_BIN, std::ios::binary);
      if (ifsKPP) ifsKPP.read(reinterpret_cast<char*>(kpp), sizeof(kpp));
      else goto Error;

    } while (0);

    return;
#else
    fstream fs;
    size_t size;

    fs.open(KPP_BIN, ios::in | ios::binary);
    if (fs.fail())
      goto Error;
    size = SQ_NB * (int)fe_end * (int)fe_end * (int)sizeof(ValueKpp);
    fs.read((char*)&kpp, size);
    if (fs.fail())
      goto Error;
    fs.close();
    size = SQ_NB * (int)SQ_NB * (int)fe_end * (int)sizeof(ValueKkp);
    fs.open(KKP_BIN, ios::in | ios::binary);
    if (fs.fail())
      goto Error;
    fs.read((char*)&kkp, size);
    if (fs.fail())
      goto Error;
    fs.close();
    fs.open(KK_BIN, ios::in | ios::binary);
    if (fs.fail())
      goto Error;
    size = SQ_NB * (int)SQ_NB * (int)sizeof(ValueKk);
    fs.read((char*)&kpp, size);
    if (fs.fail())
      goto Error;
    fs.close();
#if 0 // TODO.KkをKkpに足し込む
    {
      // 手駒の添字、コンバートするときにひとつ間違えてた。(๑´ڡ`๑)
      //ValueKpp kpp2[SQ_NB_PLUS1][fe_end][fe_end];
      //ValueKkp kkp2[SQ_NB_PLUS1][SQ_NB_PLUS1][fe_end + 1];

      ValueKkp* kkp2 = new ValueKkp[SQ_NB*(int)SQ_NB*(int)(fe_end + 1)];
      ValueKpp* kpp2 = new ValueKpp[SQ_NB*(int)fe_end*(int)(fe_end+1)];
      #define KKP2(k1,k2,p) kkp2[k1 * (int)SQ_NB*(int)(fe_end + 1) + k2 * (int)(fe_end + 1) + p ]
      #define KPP2(k,p1,p2) kpp2[k * (int)fe_end*(int)fe_end + p1 * (int)fe_end + p2 ]
      memset(kkp2, 0, sizeof(kkp));
      memset(kpp2, 0, sizeof(kpp));

      for (int k1 = 0; k1 < SQ_NB; ++k1)
        for (int k2 = 0; k2 < SQ_NB; ++k2)
          for (int j = 1; j < fe_end + 1; ++j)
          {
            int j2 = j < fe_hand_end ? j - 1 : j;
            KKP2(k1, k2, j) = kkp[k1][k2][j2];
          }

      for (int k = 0; k < SQ_NB; ++k)
        for (int i = 1; i < fe_end; ++i)
          for (int j = 1; j < fe_end; ++j)
          {
            int i2 = i < fe_hand_end ? i - 1 : i;
            int j2 = j < fe_hand_end ? j - 1 : j;
            KPP2(k,i,j) = kpp[k][i2][j2];
          }

      memcpy(kkp, kkp2, sizeof(kkp));
      memcpy(kpp, kpp2, sizeof(kpp));

      delete[] kkp2;
      delete[] kpp2;
    }
#endif
    return;
#endif
  Error:;
    cout << "\ninfo string open evaluation file failed.\n";
//    cout << "\nERROR open evaluation file failed.\n";
    // 評価関数ファイルの読み込みに失敗した場合、思考を開始しないように抑制したほうがいいと思う。
  }

  // KPP,KPのスケール
  const int FV_SCALE = 32;

#ifdef USE_EHASH
  // 評価値をcacheしておくための仕組み
  struct alignas(32) EvalHash
  {
    union {
      struct {
        HASH_KEY key;
        Value sumKKP;
        Value sumBKPP;
        Value sumWKPP;
        // 8 + 4 + 4 + 4 = 20bytes
        u8 padding[12];
      };
      ymm m;
    };

    EvalHash(){}

    // copyがatomicでないと困るのでAVX2の命令でコピーしてしまう。
    EvalHash& operator = (const EvalHash& rhs) { this->m = rhs.m; return *this; }
  };
  
  // あまり大きくしすぎるとCPU cacheの汚染がひどくなる。
  const u32 EHASH_SIZE = 1024 * 128;
  EvalHash ehash[EHASH_SIZE];
#endif

  // 駒割り以外の全計算
  // pos.st->BKPP,WKPP,KPPを初期化する。Position::set()で一度だけ呼び出される。(以降は差分計算)
  Value compute_eval(const Position& pos)
  {
    Square sq_bk = pos.king_square(BLACK);
    Square sq_wk = pos.king_square(WHITE);
    const auto* ppkppb = kpp[sq_bk];
    const auto* ppkppw = kpp[Inv(sq_wk)];

    auto& pos_ = *const_cast<Position*>(&pos);
    auto list = pos_.eval_list()->piece_list();

    int i, j;
    BonaPiece k0, k1, l0, l1;
    EvalSum sum;

    sum.p[2] = kk[sq_bk][sq_wk];

#if defined USE_AVX2_EVAL || defined USE_SSE_EVAL
    sum.m[0] = _mm_setzero_si128();
    for (i = 0; i < PIECE_NO_KING; ++i) 
    {
      k0 = list[i].fb;
      k1 = list[i].fw;
      const auto* pkppb = ppkppb[k0];
      const auto* pkppw = ppkppw[k1];
      for (j = 0; j < i; ++j) 
      {
        l0 = list[j].fb;
        l1 = list[j].fw;
        __m128i tmp;
        tmp = _mm_set_epi32(0, 0, *reinterpret_cast<const int32_t*>(&pkppw[l1][0]), *reinterpret_cast<const int32_t*>(&pkppb[l0][0]));
        tmp = _mm_cvtepi16_epi32(tmp);
        sum.m[0] = _mm_add_epi32(sum.m[0], tmp);
      }
      sum.p[2] += kkp[sq_bk][sq_wk][k0];
    }

#else
    sum.p[0][0] = 0;
    sum.p[0][1] = 0;
    sum.p[1][0] = 0;
    sum.p[1][1] = 0;

    for (i = 0; i < PIECE_NO_KING; ++i)
    {
      k0 = list[i].fb;
      k1 = list[i].fw;
      const auto* pkppb = ppkppb[k0];
      const auto* pkppw = ppkppw[k1];
      for (j = 0; j < i; ++j)
      {
        l0 = list[j].fb;
        l1 = list[j].fw;
        sum.p[0] += pkppb[l0];
        sum.p[1] += pkppw[l1];
      }
      sum.p[2] += kkp[sq_bk][sq_wk][k0];
    }

#endif
    auto& info = *pos.state();
    info.sum = sum;

#ifdef USE_EHASH

    // eval cacheに保存しておく。
    EvalHash e;
    e.sumKKP = Value(sumKKP);
    e.sumBKPP = Value(sumBKPP);
    e.sumWKPP = Value(sumWKPP);
    e.key = info.key();
    ehash[e.key & (EHASH_SIZE - 1)] = e;

#endif

    sum.p[2][0] += pos.state()->materialValue * FV_SCALE;

    return Value(sum.sum(pos.side_to_move()) / FV_SCALE);
  }
#if 1
  Value calc_diff_kpp(const Position& pos)
  {
    // 過去に遡って差分を計算していく。
    auto st = pos.state();

    // すでに計算されている。rootか？
    EvalSum sum;
    if (st->sum.p[2][0] != INT_MAX)
    {
      sum = st->sum;
      goto CALC_DIFF_END;
    }

#ifdef USE_EHASH
    HASH_KEY key = st->key();
    {
      auto e = ehash[key & (EHASH_SIZE - 1)];
      if (e.key == key)
      {
        // hitしたのでこのまま返す
        st->sumKKP = sumKKP = e.sumKKP;
        st->sumBKPP = sumBKPP = e.sumBKPP;
        st->sumWKPP = sumWKPP = e.sumWKPP;
        
        goto CALC_DIFF_END;
      }
    }
#endif

    // 遡るのは一つだけ
    // ひとつずつ遡りながらsumKPPがVALUE_NONEでないところまで探してそこからの差分を計算することは出来るが
    // レアケースだし、StateInfoにEvalListを持たせる必要が出てきて、あまり得しない。
    auto now = st;
    auto prev = st->previous;

    if (prev->sum.p[2][0] == INT_MAX)
    {
#ifdef USE_EHASH
      HASH_KEY key2 = prev->key();
      auto e = ehash[key2 & (EHASH_SIZE - 1)];
      if (e.key == key2)
      {
        // hitしたのでここからの差分計算を行なう。
        prev->sumKKP = e.sumKKP;
        prev->sumBKPP = e.sumBKPP;
        prev->sumWKPP = e.sumWKPP;
        ASSERT_LV3(e.sumKKP != INT_MAX);
      }
      else
#endif
      {
        // 全計算
        //compute_eval(pos);
        //sum = now->sum;
        //goto CALC_DIFF_END;
        return compute_eval(pos);
      }
    }

    // この差分を求める
    {
      sum = prev->sum;
      int k0, k1, k2, k3;

      auto sq_bk0 = pos.king_square(BLACK);
      auto sq_wk0 = pos.king_square(WHITE);
      auto sq_wk1 = Inv(pos.king_square(WHITE));

      auto now_list = pos.eval_list()->piece_list();

      int i, j;
      auto& dp = now->dirtyPiece;

      // 移動させた駒は最大2つある。その数
      int k = dp.dirty_num;

      auto dirty = dp.pieceNo[0];
      if (dirty >= PIECE_NO_KING) // 王と王でないかで場合分け
      {
        if (dirty == PIECE_NO_BKING)
        {
          // ----------------------------
          // 先手玉が移動したときの計算
          // ----------------------------

          // 現在の玉の位置に移動させて計算する。
          // 先手玉に関するKKP,KPPは全計算なので一つ前の値は関係ない。

          sum.p[0][0] = 0;//BKPP
          sum.p[0][1] = 0;

          // このときKKPは差分で済まない。
          sum.p[2] = Eval::kk[sq_bk0][sq_wk0];

          // 片側まるごと計算
          for (i = 0; i < PIECE_NO_KING; i++)
          {
            k0 = now_list[i].fb;
            sum.p[2] += Eval::kkp[sq_bk0][sq_wk0][k0];

            for (j = 0; j < i; j++)
            {
              sum.p[0] += Eval::kpp[sq_bk0][k0][now_list[j].fb];//BKPP
            }
          }

          // もうひとつの駒がないならこれで計算終わりなのだが。
          if (k == 2)
          {
            // この駒についての差分計算をしないといけない。
            k1 = dp.piecePrevious[1].fw;
            k3 = dp.pieceNow[1].fw;

            dirty = dp.pieceNo[1];
            // BKPPはすでに計算済みなのでWKPPのみ。
            // WKは移動していないのでこれは前のままでいい。
            for (i = 0; i < dirty; ++i)
            {
              sum.p[1] -= Eval::kpp[sq_wk1][k1][now_list[i].fw];
              sum.p[1] += Eval::kpp[sq_wk1][k3][now_list[i].fw];
            }
            for (++i; i < PIECE_NO_KING; ++i)
            {
              sum.p[1] -= Eval::kpp[sq_wk1][k1][now_list[i].fw];
              sum.p[1] += Eval::kpp[sq_wk1][k3][now_list[i].fw];
            }
          }

        } else {
          // ----------------------------
          // 後手玉が移動したときの計算
          // ----------------------------
          ASSERT_LV3(dirty == PIECE_NO_WKING);

          sum.p[1][0] = 0;//WKPP
          sum.p[1][1] = 0;
          sum.p[2] = Eval::kk[sq_bk0][sq_wk0];

          for (i = 0; i < PIECE_NO_KING; i++)
          {
            k0 = now_list[i].fb; // これ、KKPテーブルにk1側も入れておいて欲しい気はするが..
            k1 = now_list[i].fw;
            sum.p[2] += Eval::kkp[sq_bk0][sq_wk0][k0];

            for (j = 0; j < i; j++)
            {
              sum.p[1] += Eval::kpp[sq_wk1][k1][now_list[j].fw];//WKPP
            }
          }

          if (k == 2)
          {
            k0 = dp.piecePrevious[1].fb;
            k2 = dp.pieceNow[1].fb;

            dirty = dp.pieceNo[1];
            for (i = 0; i < dirty; ++i)
            {
              sum.p[0] -= Eval::kpp[sq_bk0][k0][now_list[i].fb];
              sum.p[0] += Eval::kpp[sq_bk0][k2][now_list[i].fb];
            }
            for (++i; i < PIECE_NO_KING; ++i)
            {
              sum.p[0] -= Eval::kpp[sq_bk0][k0][now_list[i].fb];
              sum.p[0] += Eval::kpp[sq_bk0][k2][now_list[i].fb];
            }
          }
        }

      } else {
        // ----------------------------
        // 玉以外が移動したときの計算
        // ----------------------------
#define ADD_BWKPP(W0,W1,W2,W3) { \
          sum.p[0] -= Eval::kpp[sq_bk0][W0][now_list[i].fb]; \
          sum.p[1] -= Eval::kpp[sq_wk1][W1][now_list[i].fw]; \
          sum.p[0] += Eval::kpp[sq_bk0][W2][now_list[i].fb]; \
          sum.p[1] += Eval::kpp[sq_wk1][W3][now_list[i].fw]; \
}
        if (k == 1)
        {
          // 移動した駒が一つ。

          k0 = dp.piecePrevious[0].fb;
          k1 = dp.piecePrevious[0].fw;
          k2 = dp.pieceNow[0].fb;
          k3 = dp.pieceNow[0].fw;

          // KKP差分
          sum.p[2] -= Eval::kkp[sq_bk0][sq_wk0][k0];
          sum.p[2] += Eval::kkp[sq_bk0][sq_wk0][k2];

          // KP値、要らんのでi==dirtyを除く
          for (i = 0; i < dirty; ++i)
            ADD_BWKPP(k0, k1, k2, k3);
          for (++i; i < PIECE_NO_KING; ++i)
            ADD_BWKPP(k0, k1, k2, k3);

        } else if (k == 2) {

          // 移動する駒が王以外の2つ。
          PieceNo dirty2 = dp.pieceNo[1];
          if (dirty > dirty2) swap(dirty, dirty2);
          // PIECE_NO_ZERO <= dirty < dirty2 < PIECE_NO_KING
          // にしておく。

          k0 = dp.piecePrevious[0].fb;
          k1 = dp.piecePrevious[0].fw;
          k2 = dp.pieceNow[0].fb;
          k3 = dp.pieceNow[0].fw;

          int m0, m1, m2, m3;
          m0 = dp.piecePrevious[1].fb;
          m1 = dp.piecePrevious[1].fw;
          m2 = dp.pieceNow[1].fb;
          m3 = dp.pieceNow[1].fw;

          // KKP差分
          sum.p[2] -= Eval::kkp[sq_bk0][sq_wk0][k0];
          sum.p[2] += Eval::kkp[sq_bk0][sq_wk0][k2];
          sum.p[2] -= Eval::kkp[sq_bk0][sq_wk0][m0];
          sum.p[2] += Eval::kkp[sq_bk0][sq_wk0][m2];

          // KPP差分
          for (i = 0; i < dirty; ++i)
          {
            ADD_BWKPP(k0, k1, k2, k3);
            ADD_BWKPP(m0, m1, m2, m3);
          }
          for (++i; i < dirty2; ++i)
          {
            ADD_BWKPP(k0, k1, k2, k3);
            ADD_BWKPP(m0, m1, m2, m3);
          }
          for (++i; i < PIECE_NO_KING; ++i)
          {
            ADD_BWKPP(k0, k1, k2, k3);
            ADD_BWKPP(m0, m1, m2, m3);
          }
          sum.p[0] -= Eval::kpp[sq_bk0][k0][m0];
          sum.p[1] -= Eval::kpp[sq_wk1][k1][m1];
          sum.p[0] += Eval::kpp[sq_bk0][k2][m2];
          sum.p[1] += Eval::kpp[sq_wk1][k3][m3];
        }
      }
    }

    now->sum = sum;
#ifdef USE_EHASH
    // せっかく計算したのでehashに保存しておく。
    {
      EvalHash e;
      e.sumKKP = Value(sumKKP);
      e.sumBKPP = Value(sumBKPP);
      e.sumWKPP = Value(sumWKPP);
      e.key = key;
      ehash[key & (EHASH_SIZE - 1)] = e;
    }
#endif

    // 差分計算終わり
CALC_DIFF_END:;

    sum.p[2][0] += pos.state()->materialValue * FV_SCALE;

    return Value(sum.sum(pos.side_to_move()) / FV_SCALE);
  }
#endif
  // 評価関数
  Value evaluate(const Position& pos)
  {
#if 1
    // 差分計算
    auto score = calc_diff_kpp(pos);
    //if (score != compute_eval(pos)) abort();
#else
    // 非差分計算
    auto score = compute_eval(pos);
#endif
    ASSERT_LV5(pos.state()->materialValue == Eval::material(pos));

    // 差分計算と非差分計算との計算結果が合致するかのテスト。(さすがに重いのでコメントアウトしておく)
    //    ASSERT_LV5(score == compute_eval(pos) + pos.state()->materialValue);

    return score;
  }

  // 現在の局面の評価値の内訳を表示する。
  void print_eval_stat(Position& pos)
  {
    cout << "--- EVAL STAT\n";

    Square sq_bk0 = pos.king_square(BLACK);
    Square sq_wk0 = pos.king_square(WHITE);
    Square sq_wk1 = Inv(pos.king_square(WHITE));

    auto list = pos.eval_list()->piece_list();
#if 0
    int i, j;
    BonaPiece k0, k1;

    // 38枚の駒を表示
    for (i = 0; i < PIECE_NO_KING; ++i)
      cout << int(list[i].fb) << " = " << list[i].fb << " , " << int(list[i].fw) << " =  " << list[i].fw << endl;

    int32_t sumBKPP, sumWKPP, sumKKP;

    cout << "KKC : " << sq_bk0 << " " << Inv(sq_wk1) << " = " << kkp[sq_bk0][sq_wk1][fe_end] << "\n";

    sumBKPP = sumWKPP = 0;
    sumKKP = kk[sq_bk0][sq_wk0][0];//kkp[sq_bk0][sq_wk1][fe_end];

    for (i = 0; i < PIECE_NO_KING; i++)
    {
      k0 = list[i].fb;
      k1 = list[i].fw;

      cout << "KKP : " << sq_bk0 << " " << Inv(sq_wk1) << " " << k0 << " = " << kkp[sq_bk0][sq_wk0][k0] << "\n";
      sumKKP += kkp[sq_bk0][sq_wk0][k0][0];

      for (j = 0; j <= i; j++)
      {
        cout << "BKPP : " << sq_bk0 << " " << k0 << " " << list[j].fb << " = " << kpp[sq_bk0][k0][list[j].fb] << "\n";
        cout << "WKPP : " << sq_wk1 << " " << k1 << " " << list[j].fw << " = " << kpp[sq_wk1][k1][list[j].fw] << "\n";

        sumBKPP += kpp[sq_bk0][k0][list[j].fb][0];
        sumWKPP += kpp[sq_wk1][k1][list[j].fw][0];

        //        cout << "sumWKPP = " << sumWKPP << " sumBKPP " << sumBKPP << " sumWKPP " << sumWKPP << endl;

        // i==jにおいて0以外やったらあかんで!!
        ASSERT(!(i == j && kpp[sq_bk0][k0][list[j].fb] != 0));
      }
    }

    cout << "Material = " << pos.state()->materialValue << endl;
    cout << "sumKKP = " << sumKKP << " sumBKPP " << sumBKPP << " sumWKPP " << sumWKPP << endl;
    cout << "---\n";
#endif
  }

}

#endif // EVAL_KPP
