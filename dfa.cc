#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <climits>//CHAR_BITの使用のため
#include <vector>//vectorの使用のため
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
int gettimeofday(struct timeval *tv,struct timezone *tz);
int settimeofday(const struct timeval *tv,const struct timezone *tz);
int getrusage(int who, struct rusage *usage);
  struct timeval tv,tv1,tz,tz1;
using namespace std;
int state_cnt=0;
int trans_cnt=0;
/*
   NFA state : 非決定性有限オートマトンの各要素、一つの状態を表している
   */
class NFAState
{
  // 文字数の定義
  // 文字のパターンが256個ある(改行やタブをなども含まれている)
  static const size_t CHARA_SIZE = 256;
  // 文字パターン charaMask_ の要素数
  static const size_t MASK_SIZE = 
    CHARA_SIZE / CHAR_BIT + ( ( CHARA_SIZE % CHAR_BIT != 0 ) ? 1 : 0 );//<条件式>?<真式>:<偽式>

  // 遷移先への相対位に対する型
  //typedef 型名 識別子; 独自の型名を付けることが出来る
  typedef vector<NFAState>::difference_type difference_type;

  vector< unsigned char > charaMask_; // 遷移できる文字パターン、二次元配列にする

  public:

  /*
     公開メンバ変数群
     */

  bool isEmpty;          // 空文字か ?

  difference_type next0; // 一つ目の遷移先
  difference_type next1; // 二つ目の遷移先

  /*
     公開メンバ関数群
     */
  vector<unsigned char> NFA_mask_chara(){
    return(charaMask_);
  }
  // コンストラクタ：初期化している
  NFAState()
    : charaMask_( MASK_SIZE, 0 ), isEmpty( true ), next0( 0 ), next1( 0 ) {}

  // clearMask : 文字パターンのビットクリア
  void clearMask()
  {
    for ( vector<unsigned char>::size_type i = 0 ;
        i < MASK_SIZE ; ++i )
      charaMask_[i] = 0;
  }

  // fillMask : 文字パターンのビットフィル
  void fillMask()
  {
    for ( vector<unsigned char>::size_type i = 0 ;
        i < MASK_SIZE ; ++i )
      charaMask_[i] = ~0;
  }

  // reverseMask : 文字パターンのビット反転
  void reverseMask()
  {
    for ( vector<unsigned char>::size_type i = 0 ;
        i < MASK_SIZE ; ++i )
      charaMask_[i] = ~( charaMask_[i] );
  }

  // setMask : 文字コードに対応する文字パターンのビットを立てる
  // 遷移する文字を決める。"a","b"など
  void setMask( unsigned char c )
  {
    size_t index = c / CHAR_BIT;
    unsigned int maskNum = c % CHAR_BIT;

    charaMask_[index] |= 1 << maskNum;
  }

  // checkChara : 文字照合ルーチン
  // setMaskで設定した文字がっているかをチェックする。
  bool checkChara( unsigned char c ) const
  {
    size_t index = c / CHAR_BIT;
    unsigned int maskNum = c % CHAR_BIT;

    return( ( ( charaMask_[index] ) & ( 1 << maskNum ) ) != 0 );
  }

  void print() const;
};

class DFAState {
  // 文字数の定義
  // 文字のパターンが256個ある(改行やタブをなども含まれている)
  static const size_t CHARA_SIZE = 256;
  // 文字パターン charaMask_ の要素数
  static const size_t MASK_SIZE = 
    CHARA_SIZE / CHAR_BIT + ( ( CHARA_SIZE % CHAR_BIT != 0 ) ? 1 : 0 );//<条件式>?<真式>:<偽式>

  typedef vector<unsigned char> vec_char;
  vector<vec_char> charaMask_; // 遷移できる文字パターン、二次元配列にする

  public:

  /*
     公開メンバ変数群
     */

  vector<unsigned int> NFA_next_trans;
  vector<unsigned int> DFA_next_trans;
  vector<unsigned int> DFA_group;

  /*
     公開メンバ関数群
     */


  // checkChara:文字照合ルーチン
  // setMaskで設定した文字がっているかをチェックする。
  bool setChara( vector < unsigned char > mask){
    charaMask_.push_back(mask);
  }

  int checkChara( unsigned char c ){
    size_t index = c / CHAR_BIT;
    unsigned int maskNum = c % CHAR_BIT;
    unsigned int count = 0;

    vector <vec_char>::iterator it = charaMask_.begin();
    for(; it < charaMask_.end(); it++){
      if( ( (*it)[index] & (1 << maskNum) )!= 0) {
        return((int)DFA_next_trans[count]);
      }
      count++;
    }
    return(-1);
  }

  void print(vector<unsigned char> cc);
};

/*
ATOM : メタ文字の定義
*/
namespace ATOM
{
  static const char REPEAT0         = '*';  // 0 回以上の繰り返し
  static const char REPEAT1         = '+';  // 1 回以上の繰り返し
  static const char REPEAT01        = '\?'; // 0 または 1 回

  static const char HEADOFPAT       = '^';  // 行頭
  static const char ENDOFPAT        = '$';  // 行末

  static const char CHARA_ANY       = '.';  // 任意の一文字
  static const char QUOTE           = '\\'; // クォート(エスケープ・シーケンス)

  static const char GROUP_START     = '(';  // グループの開始
  static const char GROUP_END       = ')';  // グループの終了

  static const char OR              = '|';  // 論理和

  static const char CLASS_START     = '[';  // ブラケット表現の開始
  static const char CLASS_END       = ']';  // ブラケット表現の終了
  static const char CLASS_REVERSE   = '^';  // ブラケット表現の反転
  static const char CLASS_SEQUENCE  = '-';  // ブラケット表現の文字範囲
};


/*
Regex : 正規表現のコンパイルと検索
*/
class Regex{
  // 型の定義
  typedef string::const_iterator StrCit;
  typedef vector<NFAState>::iterator NFAIt;
  typedef vector<NFAState>::const_iterator NFACit;
  typedef vector<NFAState>::size_type size_type;
  typedef vector<NFAState>::difference_type difference_type;

  vector<NFAState> vecNFA_; // 非決定性有限オートマトン(全ての状態を持つ完成したもの)
  vector<DFAState> vecDFA_; // 決定性有限オートマトン
  //vector< vector < unsigned int > > vecDFA_;
  bool startFlg_;           // 開始文字 (^) が先頭に指定されていたか
  bool endFlg_;             // 終了文字 ($) が末尾に指定されていたか

  bool NFA_Quote( unsigned char c );    // エスケープ系列用 NFAState 作成
  void NFA_Chara( unsigned char c );    // 文字コード用 NFAState 作成
  bool NFA_Class( StrCit s, StrCit e ); // ブラケット表現用 NFAState 作成

  bool NFA_LEOneRepeater( size_type headIdx );  // 0, 1 回の繰り返し用 NFAState 作成
  bool NFA_GEOneRepeater( size_type headIdx );  // 1 回以上の繰り返し用 NFAState 作成
  bool NFA_GEZeroRepeater( size_type headIdx ); // 0 回以上の繰り返し用 NFAState 作成

  void NFA_Or( size_type idx0, size_type idx1 ); // 論理和用 NFAState 作成

  bool compileBranch( StrCit s, StrCit e ); // 枝の構文解析 "()","{}"の処理を行い、状態を作る
  bool compile( StrCit s, StrCit e );       // 構文解析ルーチン"()","{}"があるかの探索を行う
  /*
     Regex::regexSub : 正規表現によるパターン照合(メイン・ルーチン)

     StrCit s : 照合開始位置
     StrCit e : 照合終了位置の次

     戻り値 : 一致範囲の次の位置
     */
  StrCit regexSub( StrCit s, StrCit e ) const
  {
    vector<NFACit> buffer[2]; // バッファ(切り替えのため二つ準備)
    StrCit matched = s;       // 一致範囲の次の位置
    int sw = 0;               // buffer の切り替えフラグ

    buffer[sw].push_back( vecNFA_.begin() );

    for ( ; s <= e ; ++s ) {
      for ( vector<NFACit>::size_type i = 0 ; i < buffer[sw].size() ; ++i ) {
        NFACit nfa = buffer[sw][i];
        // 終点に達したら一致範囲末尾の次のポインタを登録
        if ( nfa->next0 == 0 && nfa->next1 == 0 ) {
          matched = s;
          // 空文字遷移の場合は同じバッファに登録する
        } else if ( nfa->isEmpty ) {
          if ( nfa->next0 != 0 )
            addBuffer( buffer[sw], nfa + nfa->next0 );
          if ( nfa->next1 != 0 )
            addBuffer( buffer[sw], nfa + nfa->next1 );
          // 文字パターンがマッチしたら,もう一方のバッファに次の状態を登録
        } else if ( s < e && nfa->checkChara( *s ) ) {
          addBuffer( buffer[sw^1], nfa + nfa->next0 );
        }
      }
      buffer[sw].clear();
      sw ^= 1; // バッファの切り替え
      if ( buffer[sw].empty() ) break; // 次のバッファが空なら終了
    }

    return( matched );
  }



  bool regex( StrCit& s, StrCit& e ) const; // 文字列検索(開始位置を移動しながら regSub を呼び出す)
  bool DFA_regex( StrCit& s, StrCit& e ) const; // 文字列検索(開始位置を移動しながら regSub を呼び出す)

  static void addBuffer( vector<NFACit>& vecBuffer, NFACit nfa ); // バッファへの追加(regexSub用)

  /*
FindGroupEnd : グループ化(...)の終端を見つける

StrCit s : グループの開始 '(' がある個所
StrCit e : 探索範囲の終端の次

戻り値 : グループの終了 ')' が見つかった個所
見つからなかった場合は e が返される
*/
  StrCit FindGroupEnd( StrCit s, StrCit e ){
    int gCnt = 0; // グループが入れ子の場合、その数

    StrCit p = s; // 終端を取り出すイテレータ
    for ( ; p != e ; ++p ) {
      if ( *p == ATOM::GROUP_END )
        if ( --gCnt == 0 ) break;
      if ( *p == ATOM::GROUP_START )
        ++gCnt;
    }

    // 終端まで ')' が見つからなかった
    if ( p == e )
      cerr << "Unterminated group found." << endl;

    return( p );
  }

  /*
FindClassEnd : ブラケット表現[...]の終端を見つける

StrCit s : ブラケット表現の開始 '[' がある個所
StrCit e : 探索範囲の終端の次

戻り値 : ブラケット表現の終了 ']' が見つかった個所
見つからなかった場合は e が返される
*/
  StrCit FindClassEnd( StrCit s, StrCit e )
  {
    StrCit p = s; // 終端を取り出すイテレータ

    while ( p != e && *p != ATOM::CLASS_END )
      ++p;

    // 先頭に ']' ( または '^]' ) があった場合は再探索
    if ( p != e ) {
      if ( p == s + 1 ||                                       // '[' の次に ']' ( ブラケットの先頭が ']' )
          ( p == s + 2 && *( s + 1 ) == ATOM::CLASS_REVERSE ) // ブラケットの先頭が '^]'
         )
        for ( ++p ; p != e && *p != ATOM::CLASS_END ; ++p );
    }

    // 終端まで ']' が見つからなかった
    if ( p == e )
      cerr << "Unterminated bracket found." << endl;

    return( p );
  }

  public:

  // コンストラクタ
  Regex() : startFlg_( false ), endFlg_( false ) {}

  bool compile( const string& pattern ); // コンパイル処理

  bool regex( const string& str, StrCit& s, StrCit& e ) const; // 文字列検索
  bool DFA_regex( const string& str, StrCit& s, StrCit& e ) const; // 文字列検索

  void print() const;
  int DFA_print();

  int DFA_create (size_type st);
  void findEmptyState(size_type st);
  vector<unsigned int> vecDFA_state;//複数あるDFAの状態を表す
  vector< pair < NFAState, size_type> > vecDFA_trans;
};

/*
   Regex::compile : 構文解析ルーチン

   StrCit s : 構文の先頭
   StrCit e : 構文の末尾の次

   戻り値 : true ... 正常終了
   false ... 各文節の処理に失敗
   */
bool Regex::compile( StrCit s, StrCit e )
{
  size_type nfa0 = vecNFA_.size() - 1; // グループの先頭
  size_type nfa1 = 0;                  // グループの先頭(論理和で利用)

  //一文字ずつ解析を行う
  for ( StrCit p = s ; p != e ; ++p ) {
    switch ( *p ) {
      case ATOM::QUOTE:
        // 末尾が '\' の場合は false を返す
        if ( ++p == e ) {
          cerr << "\\ can't be the last character of regex pattern." << endl;
          return( false );
        }
        break;
      case ATOM::OR:
        if ( ! compileBranch( s, p ) ) return( false );
        if ( ( s = p + 1 ) >= e ) return( true ); // 末尾に'|'があった場合は無視
        if ( nfa1 != 0 ) NFA_Or( nfa0, nfa1 );
        nfa1 = vecNFA_.size() - 1;
        break;
      case ATOM::GROUP_START:
        // グループは compileBranch で処理するのでスキップ
        p = FindGroupEnd( p, e );
        if ( p == e ) return( false );
    }
  }
  if ( ! compileBranch( s, e ) ) return( false );
  if ( nfa1 != 0 ) NFA_Or( nfa0, nfa1 );

  //一番最後の状態
  vecNFA_.back().clearMask();
  vecNFA_.back().next0 = 0;
  vecNFA_.back().next1 = 0;

  return( true );
}

/*
   Regex::compile : 構文解析ルーチン

   const string& pattern : 正規表現

   戻り値 : true ... 正常終了
   false ... 各文節の処理に失敗
   */
bool Regex::compile( const string& pattern )
{
  vecNFA_.clear();//初期化
  vecNFA_.push_back( NFAState() );//コンストラクタを呼ぶ

  StrCit s = pattern.begin(); // pattern の開始
  StrCit e = pattern.end();   // pattern の末尾の次

  // 先頭に ^ があれば startFlg_ = true にしてスキップ
  if ( *s == ATOM::HEADOFPAT ) {
    ++s;
    startFlg_ = true;
  }

  // 末尾に $ があれば endFlg_ = true にしてスキップ
  if ( *( e - 1 ) == ATOM::ENDOFPAT ) {
    --e;
    endFlg_ = true;
  }

  return( compile( s, e ) );
}

/*
   Regex::NFA_Or : 論理和用 NFAState 作成

   size_type idx0, idx1 : 二つの枝の先頭

   idx0       idx1
   (grp0 ... ) (grp1 ...) (empty)
   v

   idx0                  idx1
   |--------v---------------------v      |---v      |---v
   <branch> (grp0 ...) <grp0_end> (grp1 ...) (grp1_end) <empty>
   |---^      |-------------------------^
   */
void Regex::NFA_Or( size_type idx0, size_type idx1 )
{
  // grp1 の先頭に grp0 の末尾を追加
  vecNFA_.insert( vecNFA_.begin() + idx1, 1, NFAState() );
  // grp0 の先頭に空分岐のための NFAState を挿入
  vecNFA_.insert( vecNFA_.begin() + idx0, 1, NFAState() );

  size_type branch = idx0;     // 先頭の空分岐

  ++idx0;    // idx0 の補正
  idx1 += 2; // idx1 の補正

  // 先頭の空分岐に分岐先を代入
  vecNFA_[branch].clearMask();
  vecNFA_[branch].next0 = 1;
  vecNFA_[branch].next1 = idx1 - branch;

  // 二つの枝の末尾の分岐先を最後の空分岐に
  vecNFA_[idx1 - 1].clearMask();
  vecNFA_[idx1 - 1].next0 = vecNFA_.size() - ( idx1 - 1 );
  vecNFA_[idx1 - 1].next1 = 0;
  vecNFA_[vecNFA_.size() - 1].clearMask();
  vecNFA_[vecNFA_.size() - 1].next0 = 1;
  vecNFA_[vecNFA_.size() - 1].next1 = 0;

  // 次の要素を用意しておく
  vecNFA_.push_back( NFAState() );

  return;
}


/*
   Regex::compileBranch : 枝の構文解析

   StrCit s : 枝の先頭
   StrCit e : 枝の末尾の次

   戻り値 : true ... 正常終了
   false ... 各文節の処理に失敗, '\' が末尾にあった
   */
bool Regex::compileBranch( StrCit s, StrCit e )
{
  size_type headIdx = vecNFA_.size() - 1; // グループの先頭
  StrCit p1;                              // 文字列探索用ポインタ

  const string anyChara = "^\n"; // 前キャラクタ (\nを除く)

  for ( StrCit p0 = s ; p0 != e ; ++p0 ) {
    switch ( *p0 ) {
      case ATOM::CHARA_ANY:
        headIdx = vecNFA_.size() - 1;
        if ( ! NFA_Class( anyChara.begin(), anyChara.end() ) ) return( false );
        break;
      case ATOM::REPEAT0:
        if ( ! NFA_GEZeroRepeater( headIdx ) ) return( false );
        headIdx = vecNFA_.size() - 1;
        break;
      case ATOM::REPEAT1:
        if ( ! NFA_GEOneRepeater( headIdx ) ) return( false );
        headIdx = vecNFA_.size() - 1;
        break;
      case ATOM::REPEAT01:
        if ( ! NFA_LEOneRepeater( headIdx ) ) return( false );
        headIdx = vecNFA_.size() - 1;
        break;
      case ATOM::GROUP_START:
        // グループの終端 ')' を探す
        p1 = FindGroupEnd( p0, e );
        if ( p1 == e ) return( false );
        headIdx = vecNFA_.size() - 1;
        if ( ! compile( p0 + 1, p1 ) ) return( false );
        p0 = p1;
        break;
      case ATOM::CLASS_START:
        // ブラケット表現の終端 ']' を探す
        p1 = FindClassEnd( p0, e );
        if ( p1 == e ) return( false );
        headIdx = vecNFA_.size() - 1;
        if ( ! NFA_Class( p0 + 1, p1 ) ) return( false );
        p0 = p1;
        break;
      case ATOM::QUOTE:
        // 末尾が '\' の場合は false を返す
        if ( ++p0 == e ) {
          cerr << "\\ can't be the last character of regex pattern." << endl;
          return( false );
        }
        headIdx = vecNFA_.size() - 1;
        NFA_Quote( *p0 );
        break;
      default:
        headIdx = vecNFA_.size() - 1;
        NFA_Chara( *p0 );
    }
  }

  return( true );
}
/*
   エスケープ・キャラクタの定義
   */
struct ESCAPE_CHARA {
  char meta_chara;   // メタキャラクタ
  string mask_chara; // 対象文字
} escape_chara[] = {
  { 'd', "0-9", },         // 数値
  { 'D', "^0-9", },        // 数値以外
  { 'w', "a-zA-Z0-9", },   // 英数字
  { 'W', "^a-zA-Z0-9", },  // 英数字以外
  { 's', " \t\f\r\n", },   // 空白文字
  { 'S', "^ \t\f\r\n", },  // 空白文字以外
  { 'h', " \t", },         // 水平空白文字
  { 'H', "^ \t", },        // 水平空白文字以外
  { 'v', "\n\v\f\r", },  // 垂直空白文字
  { 'V', "^\n\v\f\r", }, // 垂直空白文字以外

  { 'a', "\a", }, // アラーム(BEL=0x07)
  { 'b', "\b", }, // バック・スペース(BS=0x08)
  { 't', "\t", }, // 水平タブ(HT=0x09)
  { 'n', "\n", }, // 改行(LF=0x0A)
  { 'v', "\v", }, // 垂直タブ(VT=0x0B)
  { 'f', "\f", }, // 書式送り(FF=0x0C)
  { 'r', "\r", }, // 復帰(CR=0x0D)
  { 0, "", },
};


/*
   Regex::NFA_Quote : エスケープ系列用 NFAState 作成

   unsigned char c : エスケープ・シーケンス内の文字

   戻り値 : 常に true
   */
bool Regex::NFA_Quote( unsigned char c )
{
  // エスケープ系列に該当するか
  for ( struct ESCAPE_CHARA* esc = escape_chara ; esc->meta_chara != 0 ; ++esc )
    if ( esc->meta_chara == c )
      return( NFA_Class( ( esc->mask_chara ).begin(), ( esc->mask_chara ).end() ) );

  // 該当なしの場合はキャラクタをそのまま処理
  NFA_Chara( c );

  return( true );
}

/*
   Regex::NFA_Class : ブラケット表現用 NFAState 作成

   StrCit s : ブラケット内部の開始 ( '[' の次 )
   StrCit e : ブラケット終端 ( ']' のある個所 )

   戻り値 : true ... 正常終了 false ... 文字範囲の指定が不正

   (empty)
   v

   |------v
   (back) <empty>
   ->set flag
   */
bool Regex::NFA_Class( StrCit s, StrCit e )
{
  // 末尾の要素を初期化
  NFAState& back = vecNFA_.back();
  back.clearMask();

  // '^' が指定されているかチェック
  bool revFlg = false;
  if ( *s == ATOM::CLASS_REVERSE ) {
    revFlg = true;
    ++s;
  }

  for ( StrCit p = s ; p != e ; ++p ) {
    // 照合順序の処理
    if ( p > s && ( p + 1 ) < e && *p == ATOM::CLASS_SEQUENCE ) {
      // 'x-y-z'のチェック
      if ( ( p + 3 ) < e && *( p + 2 ) == ATOM::CLASS_SEQUENCE ) {
        cerr << "Illegal range format in bracket (x-y-z)." << endl;
        return( false );
      }
      // 順序が逆ならエラー
      if ( *( p - 1 ) > *( p + 1 ) ) {
        cerr << "Reverse sequence found in bracket (y-x)." << endl;
        return( false );
      }
      for ( char c = *( p - 1 ) + 1 ; c <= *( p + 1 ) ; ++c )
        back.setMask( c );
      ++p;
    } else {
      back.setMask( *p );
    }
  }
  if ( revFlg ) back.reverseMask();

  // 左のみ次の要素に接続
  back.next0 = 1;
  back.next1 = 0;
  back.isEmpty = false;

  // 次の要素を用意しておく
  vecNFA_.push_back( NFAState() );

  return( true );
}

/*
   Regex::NFA_Chara : 文字コード用 NFAState 作成

   char c : 登録する文字コード

   (empty)
   v

   |------v
   (back) <empty>
   ->c
   */
void Regex::NFA_Chara( unsigned char c )
{
  NFAIt back = vecNFA_.end() - 1;
  back->setMask( c );
  back->next0 = 1;
  back->next1 = 0;
  back->isEmpty = false;

  // 次の要素を用意しておく
  vecNFA_.push_back( NFAState() );

  return;
}

/*
   Regex::NFA_LEOneRepeater : 0,1 回の繰り返し用 NFAState 作成

   size_type headIdx : 枝の先頭

   戻り値 : true ... 正常終了 false ... 繰り返し対象がない

   headIdx
   (grp...) (empty)
   v

   headIdx +1       +n+1
   |----v--------v
   <b0> (grp...) (empty)
   */
bool Regex::NFA_LEOneRepeater( size_type headIdx )
{
  // 繰り返し対象がない場合は false を返す
  if ( headIdx + 1 == vecNFA_.size() ) {
    cerr << "No group of repeat found." << endl;
    return( false );
  }

  // 繰り返し対象の大きさ
  difference_type n = vecNFA_.size() - headIdx - 1;

  // 先頭に b0 を挿入
  vecNFA_.insert( vecNFA_.begin() + headIdx, 1, NFAState() );

  // b0 の初期化
  NFAIt b0 = vecNFA_.begin() + headIdx;
  b0->clearMask();
  b0->next0 = 1;
  b0->next1 = n + 1;

  return( true );
}

/*
   Regex::NFA_GEOneRepeater : 1 回以上の繰り返し用 NFAState 作成

   size_type headIdx : 枝の先頭

   戻り値 : true ... 正常終了 false ... 繰り返し対象がない

   headIdx
   (grp...) (empty)
   v

   headIdx  +n   +n+1
   v--------|----v
   (grp...) <b0> <empty>
   */
bool Regex::NFA_GEOneRepeater( size_type headIdx )
{
  // 繰り返し対象がない場合は false を返す
  if ( headIdx + 1 == vecNFA_.size() ) {
    cerr << "No group of repeat found." << endl;
    return( false );
  }

  // 繰り返し対象の大きさ
  difference_type n = vecNFA_.size() - headIdx - 1;

  // b0 の初期化
  NFAIt b0 = vecNFA_.end() - 1;
  b0->clearMask();
  b0->next0 = -n;
  b0->next1 = 1;

  // 次の要素を用意しておく
  vecNFA_.push_back( NFAState() );

  return( true );
}

/*
   Regex::NFA_GEZeroRepeater : 0 回以上の繰り返し用 NFAState 作成

   size_type headIdx : 枝の先頭

   戻り値 : true ... 正常終了 false ... 繰り返し対象がない

   headIdx
   (grp...) (empty)
   v

   headIdx +1       +n+1 +n+2
   |------v-------------v
   <b0>   (grp...) (b1) <empty>
   ^--------|----^
   */
bool Regex::NFA_GEZeroRepeater( size_type headIdx ){
  // 繰り返し対象がない場合は false を返す
  if ( headIdx + 1 == vecNFA_.size() ) {
    cerr << "No group of repeat found." << endl;
    return( false );
  }

  // 繰り返し対象の大きさ
  difference_type n = vecNFA_.size() - headIdx - 1;

  // 先頭に b0 を挿入
  vecNFA_.insert( vecNFA_.begin() + headIdx, 1, NFAState() );

  // b0 の初期化
  NFAIt b0 = vecNFA_.begin() + headIdx;
  b0->clearMask();
  b0->next0 = 1;
  b0->next1 = n + 2;

  // b1 の初期化
  NFAIt b1 = vecNFA_.end() - 1;
  b1->clearMask();
  b1->next0 = -n;
  b1->next1 = 1;

  // 次の要素を用意しておく
  vecNFA_.push_back( NFAState() );

  return( true );
}
/*
NFA_Print : NFAState マスクパターンの出力 (一文字出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t index : ビット位置
const string& format : 出力フォーマット
*/
void NFA_Print( const vector<unsigned char>& charaMask, size_t index, const string& format ){
  vector<unsigned char>::size_type vecIdx = index / CHAR_BIT;
  vector<unsigned char>::size_type maskIdx = index % CHAR_BIT;

  if ( ( ( charaMask[vecIdx] >> maskIdx ) & 1 ) != 0 )
    printf( format.c_str(), index );
}

/*
NFA_Print : NFAState マスクパターンの出力 (範囲指定時の一文字出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t index : ビット位置
const string& format : 出力フォーマット
int bitOnIdx : 前にビットが立っていた位置

戻り値 : 更新した bitOnIdx
*/
int NFA_Print( const vector<unsigned char>& charaMask, size_t index, const string& format, int bitOnIdx ){
  vector<unsigned char>::size_type vecIdx = index / CHAR_BIT;
  vector<unsigned char>::size_type maskIdx = index % CHAR_BIT;

  if ( ( ( charaMask[vecIdx] >> maskIdx ) & 1 ) != 0 ) {
    if ( bitOnIdx < 0 ) {
      printf( format.c_str(), index );
      bitOnIdx = index;
    }
    return( bitOnIdx );
  } else {
    if ( bitOnIdx >= 0 ) {
      if ( (size_t)( bitOnIdx + 1 ) < index ) printf( ( "-" + format + " " ).c_str(), index - 1 );
    }
    return( -1 );
  }
}

/*
NFA_Print : NFAState マスクパターンの出力 (範囲出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t s : 開始ビット位置
size_t e : 終了ビット位置の次
const string& format : 出力フォーマット
*/
void NFA_Print( const vector<unsigned char>& charaMask, size_t s, size_t e, const string& format ){
  int bitOnIdx = -1;

  for ( size_t i = s ; i < e ; ++i )
    bitOnIdx = NFA_Print( charaMask, i, format, bitOnIdx );

  if ( bitOnIdx >= 0 && (size_t)( bitOnIdx + 1 ) < e )
    printf( ( "-" + format ).c_str(), e - 1 );
}
/*
DFA_Print : NFAState マスクパターンの出力 (一文字出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t index : ビット位置
const string& format : 出力フォーマット
*/
void DFA_Print( const vector<unsigned char>& charaMask, size_t index, const string& format ){
  vector<unsigned char>::size_type vecIdx = index / CHAR_BIT;
  vector<unsigned char>::size_type maskIdx = index % CHAR_BIT;

  if ( ( ( charaMask[vecIdx] >> maskIdx ) & 1 ) != 0 )
    printf( format.c_str(), index );
}

/*
DFA_Print : NFAState マスクパターンの出力 (範囲指定時の一文字出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t index : ビット位置
const string& format : 出力フォーマット
int bitOnIdx : 前にビットが立っていた位置

戻り値 : 更新した bitOnIdx
*/
int DFA_Print( const vector<unsigned char>& charaMask, size_t index, const string& format, int bitOnIdx ){
  vector<unsigned char>::size_type vecIdx = index / CHAR_BIT;
  vector<unsigned char>::size_type maskIdx = index % CHAR_BIT;

  if ( ( ( charaMask[vecIdx] >> maskIdx ) & 1 ) != 0 ) {
    if ( bitOnIdx < 0 ) {
      printf( format.c_str(), index );
      bitOnIdx = index;
    }
    return( bitOnIdx );
  } else {
    if ( bitOnIdx >= 0 ) {
      if ( (size_t)( bitOnIdx + 1 ) < index ) printf( ( "-" + format + " " ).c_str(), index - 1 );
    }
    return( -1 );
  }
}

/*
DFA_Print : NFAState マスクパターンの出力 (範囲出力)

const vector<unsigned char>& charaMask : マスクパターン
size_t s : 開始ビット位置
size_t e : 終了ビット位置の次
const string& format : 出力フォーマット
*/
void DFA_Print( const vector<unsigned char>& charaMask, size_t s, size_t e, const string& format ){
  int bitOnIdx = -1;

  for ( size_t i = s ; i < e ; ++i )
    bitOnIdx = DFA_Print( charaMask, i, format, bitOnIdx );

  if ( bitOnIdx >= 0 && (size_t)( bitOnIdx + 1 ) < e )
    printf( ( "-" + format ).c_str(), e - 1 );
}
/*
   NFAState::print : NFAState のマスクパターンを表示する
   */
void NFAState::print() const{
  if ( isEmpty ) return;

  NFA_Print( charaMask_, 0, '\a', "0x%02X" ); // control code 0x00-0x08

  NFA_Print( charaMask_, '\a', "\\a" ); // BEL
  NFA_Print( charaMask_, '\b', "\\b" ); // BS
  NFA_Print( charaMask_, '\t', "\\t" ); // HT
  NFA_Print( charaMask_, '\n', "\\n" ); // LF
  NFA_Print( charaMask_, '\v', "\\v" ); // VT
  NFA_Print( charaMask_, '\f', "\\f" ); // FF
  NFA_Print( charaMask_, '\r', "\\r" ); // CR

  NFA_Print( charaMask_, 0x0E, ' ', "0x%02X" ); // control code 0x0E-0x1F

  NFA_Print( charaMask_, ' ', "(SPACE)" ); // SPACE

  for ( unsigned char c = '!' ; c < '0' ; ++c )
    NFA_Print( charaMask_, c, "%c" ); // 0x21-0x2F

  NFA_Print( charaMask_, '0', ':', "%c" ); // 数値

  for ( unsigned char c = ':' ; c < 'A' ; ++c )
    NFA_Print( charaMask_, c, "%c" ); // 0x3A-0x40

  NFA_Print( charaMask_, 'A', '[', "%c" ); // 英大文字

  for ( unsigned char c = '[' ; c < 'a' ; ++c )
    NFA_Print( charaMask_, c, "%c" ); // 0x5B-0x60

  NFA_Print( charaMask_, 'a', '{', "%c" ); // 英小文字

  for ( unsigned char c = '{' ; c < 0x7F ; ++c )
    NFA_Print( charaMask_, c, "%c" ); // 0x7B-0x7E

  NFA_Print( charaMask_, 0x7F, "(DEL)" ); // DEL

  NFA_Print( charaMask_, 0x80, 0x100, "0x%02X" ); // control code 0x80-0xFF
}

void DFAState::print(vector<unsigned char>cc){

  DFA_Print( cc, 0, '\a', "0x%02X" ); // control code 0x00-0x08

  DFA_Print( cc, '\a', "\\a" ); // BEL
  DFA_Print( cc, '\b', "\\b" ); // BS
  DFA_Print( cc, '\t', "\\t" ); // HT
  DFA_Print( cc, '\n', "\\n" ); // LF
  DFA_Print( cc, '\v', "\\v" ); // VT
  DFA_Print( cc, '\f', "\\f" ); // FF
  DFA_Print( cc, '\r', "\\r" ); // CR

  DFA_Print( cc, 0x0E, ' ', "0x%02X" ); // control code 0x0E-0x1F

  DFA_Print( cc, ' ', "(SPACE)" ); // SPACE

  for ( unsigned char c = '!' ; c < '0' ; ++c )
    DFA_Print( cc, c, "%c" ); // 0x21-0x2F

  DFA_Print( cc, '0', ':', "%c" ); // 数値

  for ( unsigned char c = ':' ; c < 'A' ; ++c )
    DFA_Print( cc, c, "%c" ); // 0x3A-0x40

  DFA_Print( cc, 'A', '[', "%c" ); // 英大文字

  for ( unsigned char c = '[' ; c < 'a' ; ++c )
    DFA_Print( cc, c, "%c" ); // 0x5B-0x60

  DFA_Print( cc, 'a', '{', "%c" ); // 英小文字

  for ( unsigned char c = '{' ; c < 0x7F ; ++c )
    DFA_Print( cc, c, "%c" ); // 0x7B-0x7E

  DFA_Print( cc, 0x7F, "(DEL)" ); // DEL

  DFA_Print( cc, 0x80, 0x100, "0x%02X" ); // control code 0x80-0xFF
}




/*
   Regex::print : NFA の表示ルーチン
   */
void Regex::print() const{
  for ( size_type st = 0 ; st < vecNFA_.size() ; ++st ) {
    printf( "%.3u : mask=", (unsigned int)st );
    vecNFA_[st].print();
    cout << " next0=";
    if ( vecNFA_[st].next0 != 0 )
      cout << st + vecNFA_[st].next0;
    else
      cout << "NULL";
    cout << " next1=";
    if ( vecNFA_[st].next1 != 0 )
      cout << st + vecNFA_[st].next1;
    else
      cout << "NULL";
    cout << endl;
  }
}

/*
   Regex::addBuffer : バッファへの登録(重複チェック付き)

   vector<NFACit>& vecBuffer : 対象のバッファ
   NFACit nfa : 登録対象の NFACit
   */
void Regex::addBuffer( vector<NFACit>& vecBuffer, NFACit nfa ){
  for ( vector<NFACit>::iterator it = vecBuffer.begin() ;
      it != vecBuffer.end() ; ++it )
    if ( *it == nfa ) return;

  vecBuffer.push_back( nfa );
}


/*
   Regex::regex : 文字列の先頭ポインタを後ろにずらしながら
   パターン照合ルーチンを呼び出す

   StrCit& s : 照合開始位置
   StrCit& e : 照合終了位置の次

   戻り値 : 一致したか (一致文字列が NULL の場合は false となる)
   */
bool Regex::regex( StrCit& s, StrCit& e ) const{
  for ( ; s < e ; ++s ) {
    StrCit matched = regexSub( s, e );
    if ( matched != s ) {
      // 行末で終わっているか?
      if ( ( ! endFlg_ ) || matched == e ) {
        e = matched;
        return( true );
      }
    }
    // 行頭を示すメタキャラクタがあった場合は一度だけ処理
    if ( startFlg_ ) return( false );
  }

  return( false );
}

/*
   Regex::regex : パターン照合ルーチン

   const string& str : 照合する文字列
   StrCit& s, e : 一致範囲 ( e は末尾の次の位置 )

   戻り値 : 一致したか (一致文字列が NULL の場合は false となる)
   */
bool Regex::regex( const string& str, StrCit& s, StrCit& e ) const{
  if ( str.size() == 0 ) return( false );

  s = str.begin();
  e = str.end();

  return( regex( s, e ) );
}

bool Regex::DFA_regex( StrCit& s, StrCit& e ) const{
  for ( ; s < e ; ++s ) {
    StrCit matched = regexSub( s, e );
    if ( matched != s ) {
      // 行末で終わっているか?
      if ( ( ! endFlg_ ) || matched == e ) {
        e = matched;
        return( true );
      }
    }
    // 行頭を示すメタキャラクタがあった場合は一度だけ処理
    if ( startFlg_ ) return( false );
  }
  gettimeofday(&tz1,NULL);

  return( false );
}

bool Regex::DFA_regex( const string& str, StrCit& s, StrCit& e ) const{
  gettimeofday(&tv1,NULL);
  if ( str.size() == 0 ) return( false );

  s = str.begin();
  e = str.end();

  return( DFA_regex( s, e ) );
}
//空遷移するものを集める関数(NFA→DFA)
void Regex::findEmptyState(size_type st){
  //DFAの状態を示すためにデータを入れていく
  vecDFA_state.push_back(st);
  // 空文字遷移かどうかチェックする
  if ( vecNFA_[st].isEmpty ) {
    if ( vecNFA_[st].next0 != 0 )
      findEmptyState( st + vecNFA_[st].next0 );
    if ( vecNFA_[st].next1 != 0 )
      findEmptyState( st + vecNFA_[st].next1 );
  }else{
    vecDFA_trans.push_back(pair< NFAState, size_type >(vecNFA_[st], st));
  }
}

int Regex::DFA_create(size_type st){
  DFAState dfa;
  vecDFA_state.clear();
  vecDFA_trans.clear();
  findEmptyState(st);

  int cnt=0;
  int a=0;
  int b=0;
  int res=0;

  sort(vecDFA_state.begin(),vecDFA_state.end());
  //新しくとってきた状態が前の状態と同じかをチェック
  vector<DFAState>::iterator it_vec = vecDFA_.begin();
  for(; it_vec < vecDFA_.end(); it_vec++){
    if(vecDFA_state == it_vec->DFA_group){
      return 0;
    }
  }

  //vecDFA_stateの中身を一度DFA_groupに入れる
  vector<unsigned int>::iterator it_tmp = vecDFA_state.begin();
  for(; it_tmp < vecDFA_state.end(); it_tmp++){
    dfa.DFA_group.push_back(*it_tmp);
  }

  //DFA_groupの中身を表示する。
  vector<unsigned int>::iterator it_show = dfa.DFA_group.begin();
  for(; it_show < dfa.DFA_group.end(); it_show++){
    cout << *it_show << endl;
  }
  state_cnt++;
  //vecDFA_transにある遷移先の情報をNFA_next_transに入れる
  vector<pair <NFAState, size_type> >::iterator it_insert = vecDFA_trans.begin();
  for(; it_insert < vecDFA_trans.end(); it_insert++){
    //dfa.setChara( (it_insert->first).NFA_mask_chara() );
    dfa.NFA_next_trans.push_back( (unsigned int)((it_insert->first).next0 + it_insert->second) );
    cout <<  "mask" << a << "=";
    (it_insert->first).print();//文字の表示
    cout<< "\t" << "next" << b << "=" << dfa.NFA_next_trans[cnt]<<endl ;
    cnt++;
    a++;
    b++;
    trans_cnt++;
  }
  cout << endl;

  //vecDFA_にDFAState(class)を入れる
  vecDFA_.push_back(dfa);

  //NFA_next_transの中身を使ってfindEmptyState関数を使う
  vector<unsigned int>::iterator ita = dfa.NFA_next_trans.begin();
  for(; ita < dfa.NFA_next_trans.end(); ita++){
    DFA_create(*ita);
  }
}
int main(){
  struct rusage t;
  Regex reg;
  std::ifstream File("100kb.txt");
  std::string a(""), b("");
  string::const_iterator s,e;
  File >> a;
  cout << "input string" << endl;
  //cout << " " <<  a << endl;
  cout << "input regex" << endl;
  cin >> b;
  //b="(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a(a|b)*a";
  cout << "\nregex:" << b << endl;
  gettimeofday(&tv, NULL);
  reg.compile(b);
  reg.DFA_create(0);
  gettimeofday(&tz, NULL);
  if(reg.DFA_regex(a,s,e) == 1){
    cout << "matching!!!(DFA)\n";
  }else{
    cout << "false(DFA)\n";
  }
  printf("time = %f秒\n",(tz.tv_sec - tv.tv_sec)+(tz.tv_usec - tv.tv_usec)*1.0E-6);
  printf("matching time = %f秒\n",(tz1.tv_sec - tv1.tv_sec)+(tz1.tv_usec - tv1.tv_usec)*1.0E-6);
  getrusage(RUSAGE_SELF ,&t);
  printf("memory = %ld\n",t.ru_ixrss);
  printf("状態数 = %d\n",state_cnt);
  printf("遷移数 = %d\n",trans_cnt);
  return 0;
}
