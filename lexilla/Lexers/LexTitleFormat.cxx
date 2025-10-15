// Scintilla source code edit control
/** @file LexTitleFormat.cxx
 ** Lexer for Title Format.
 **/
// Based on Holger Stenger Title Format lexer
// Modified for Scintilla by Dayuyu, 2025
// The License.txt file describes the conditions under which this software may be distributed.

#include <cassert>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <algorithm>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"
#include "OptionSet.h"
#include "DefaultLexer.h"

using namespace Scintilla;
using namespace Lexilla;

namespace {

	// Use an unnamed namespace to protect the functions and classes from name conflicts

	class ILexerTitleformatPrivateCall {

	public:
		virtual int SCI_METHOD Version() const = 0;
		virtual void SCI_METHOD Release() = 0;
		virtual void SCI_METHOD ClearInactiveRanges() = 0;
		virtual void SCI_METHOD SetInactiveRanges(int count, Sci_CharacterRange* ranges) = 0;
		//void GetFactoryLexerTitleFormat(void * lparam);
	};

	class Range {
	public:
		Sci_Position start;
		Sci_Position end;

		Range(Sci_Position pos = 0) :
			start(pos), end(pos) {
		}

		Range(Sci_Position start_, Sci_Position end_) :
			start(start_), end(end_) {
		}

		// Is the character after pos within the range?
		bool ContainsCharacter(Sci_Position pos) const {
			return (pos >= start && pos < end);
		}
	};

	class LexerTitleformatPrivateCall : public ILexerTitleformatPrivateCall {

	public:
		LexerTitleformatPrivateCall() { refCount = 1; }
		virtual ~LexerTitleformatPrivateCall() {};

		int SCI_METHOD Version() const {
			return lvRelease5;/*ltfvOriginal*/
		}

		void AddRef() noexcept {
			++refCount;
		}

		void SCI_METHOD Release() {
			int curRefCount = --refCount;
			if (curRefCount == 0) {
				delete this;
			}
		}

		void SCI_METHOD ClearInactiveRanges() {
			inactiveRanges.clear();
		}

		void SCI_METHOD SetInactiveRanges(int count, Sci_CharacterRange* ranges) {
			inactiveRanges.resize(count);
			for (int index = 0; index < count; ++index) {
				inactiveRanges[index] = Range(ranges[index].cpMin, ranges[index].cpMax);
			}
		}

		int GetInactiveRangeCount() const {
			return inactiveRanges.size();
		}

		bool InactiveRangeContainsCharacter(int index, Sci_Position pos) const {
			if ((index < 0) || (index >= inactiveRanges.size())) {
				return false;
			}
			else {
				return inactiveRanges[index].ContainsCharacter(pos);
			}
		}

		int GetInactiveRangeLowerBound(Sci_Position pos) const {
			//TODO: pAccess->GetCharRange...
			struct pred {
				bool operator ()(const Range& range, Sci_Position pos) {
					return range.end < pos;
				}
			};
			auto it = std::lower_bound(inactiveRanges.begin(), inactiveRanges.end(), pos, pred());
			return it - inactiveRanges.begin();
		}

	private:
		int refCount;
		std::vector<Range> inactiveRanges;
	};

	// Options used for Lexer
	struct OptionsTitleformat {
		bool fold;
		OptionsTitleformat();
	};

	OptionsTitleformat::OptionsTitleformat() {
		fold = false;
	}

	const char* wordListsDesc[] = {
		"Control flow functions",
		"Variable access functions",
		"Built-in functions",
		"Third-party functions",
		nullptr,
	};

	struct OptionSetTitleformat : public OptionSet<OptionsTitleformat> {
		OptionSetTitleformat() {
			DefineProperty("fold", &OptionsTitleformat::fold);
			DefineWordListSets(wordListsDesc);
		}
	};

	bool IsAWordChar(int ch) noexcept {
		return isalnum(ch) || ch == '_';
	}

	constexpr  bool IsALiteralStringChar(int ch) noexcept {
		return ch != '$' && ch != '%' && ch != '\'' &&
			ch != '(' && ch != ')' && ch != ',' &&
			ch != '[' && ch != ']' &&
			ch != '\r' && ch != '\n';
	}

	constexpr  bool IsASpecialStringChar(int ch) noexcept {
		return ch == '$' || ch == '%' || ch == '\'';
	}

	constexpr  bool IsTitleFormatOperator(int ch) noexcept {
		if (ch == '(' || ch == ')' || ch == ',' ||
			ch == '[' || ch == ']') {
			return true;
		}
		return false;
	}

	// Lexer TitleFormat SCLEX_TITLEFORMAT SCE_TITLEFORMAT_:
	LexicalClass lexicalClasses[] = {
		0, "SCE_TITLEFORMAT_DEFAULT", "default", "White space",
		1, "SCE_TITLEFORMAT_COMMENT", "comment", "Comment",
		2, "SCE_TITLEFORMAT_OPERATOR", "operator", "Operator",
		3, "SCE_TITLEFORMAT_IDENTIFIER", "identifier", "Identifier",
		4, "SCE_TITLEFORMAT_STRING", "string","String",
		5, "SCE_TITLEFORMAT_LITERALSTRING", "literal string", "Literal string",
		6, "SCE_TITLEFORMAT_SPECIALSTRING", "special string", "Special string",
		7, "SCE_TITLEFORMAT_FIELD", "field", "Field",
		8, "SCE_TITLEFORMAT_KEYWORD1", "Keyword1", "Keyword1",
		9, "SCE_TITLEFORMAT_KEYWORD2", "Keyword2", "Keyword2",
		10, "SCE_TITLEFORMAT_KEYWORD3", "Keyword3", "keyword3",
		11, "SCE_TITLEFORMAT_KEYWORD4", "Keyword4", "Keyword4"
	};

	class LexerTitleFormat : public DefaultLexer {
		WordList keywordsPrimary;
		WordList keywordsSecondary;
		WordList keywordsTertiary;
		WordList keywordsTypes;
		OptionsTitleformat options;
		OptionSetTitleformat osTitleFormat;

	public:
		LexerTitleFormat(const char* languageName_, int language_) :
			DefaultLexer(languageName_, language_, lexicalClasses, std::size(lexicalClasses)) {

			privateCall = new LexerTitleformatPrivateCall();

		}
		// Deleted so LexerTitleFormat objects can not be copied.
		LexerTitleFormat(const LexerTitleFormat&) = delete;
		LexerTitleFormat(LexerTitleFormat&&) = delete;
		void operator=(const LexerTitleFormat&) = delete;
		void operator=(LexerTitleFormat&&) = delete;

		//~LexerTitleFormat() override = default;
		~LexerTitleFormat() {
			if (privateCall != 0) {
				privateCall->Release();
				privateCall = 0;
			}
		}

		void SCI_METHOD Release() override {
			delete this;
		}
		int SCI_METHOD Version() const override {
			return lvRelease5;
		}
		const char* SCI_METHOD PropertyNames() override {
			return osTitleFormat.PropertyNames();
		}
		int SCI_METHOD PropertyType(const char* name) override {
			return osTitleFormat.PropertyType(name);
		}
		const char* SCI_METHOD DescribeProperty(const char* name) override {
			return osTitleFormat.DescribeProperty(name);
		}
		Sci_Position SCI_METHOD PropertySet(const char* key, const char* val) override {
			if (osTitleFormat.PropertySet(&options, key, val)) {
				return 0;
			}
			return -1;
		}
		const char* SCI_METHOD PropertyGet(const char* key) override {
			return osTitleFormat.PropertyGet(key);
		}
		const char* SCI_METHOD DescribeWordListSets() override {
			return osTitleFormat.DescribeWordListSets();
		}
		Sci_Position SCI_METHOD WordListSet(int n, const char* wl) {

			WordList* wordListN = nullptr;
			switch (n) {
			case 0:
				wordListN = &keywordsPrimary;
				break;
			case 1:
				wordListN = &keywordsSecondary;
				break;
			case 2:
				wordListN = &keywordsTertiary;
				break;
			case 3:
				wordListN = &keywordsTypes;
				break;
			default:
				break;
			}
			Sci_Position firstModification = -1;
			if (wordListN && wordListN->Set(wl, false)) {
				firstModification = 0;
			}
			return firstModification;
		}

		void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument* pAccess) {

			Accessor styler(pAccess, nullptr);

			struct pred {
				bool operator ()(const std::pair<int, int>& range, int pos) {
					return range.first < pos;
				}
			};

			StyleContext sc(startPos, lengthDoc, initStyle, styler);

			int rangeCount = privateCall->GetInactiveRangeCount();
			int currentRange = privateCall->GetInactiveRangeLowerBound(sc.currentPos);

			int inactiveFlag = 0;

			for (; sc.More(); sc.Forward()) {
				// Determine if the current state should terminate.
				switch (sc.state & 63) {
				case SCE_TITLEFORMAT_OPERATOR:
					sc.SetState(SCE_TITLEFORMAT_DEFAULT);
					break;
				case SCE_TITLEFORMAT_IDENTIFIER:
					if (!IsAWordChar(sc.ch)) {
						sc.SetState(SCE_TITLEFORMAT_DEFAULT);
					}
					break;
				case SCE_TITLEFORMAT_COMMENT:
					if (sc.atLineStart) {
						sc.SetState(SCE_TITLEFORMAT_DEFAULT);
					}
					break;
				case SCE_TITLEFORMAT_STRING:
					if (sc.ch == '\'') {
						sc.ForwardSetState(SCE_TITLEFORMAT_DEFAULT);
					}
					break;
				case SCE_TITLEFORMAT_LITERALSTRING:
					if (!IsALiteralStringChar(sc.ch)) {
						sc.SetState(SCE_TITLEFORMAT_DEFAULT);
					}
					break;
				case SCE_TITLEFORMAT_SPECIALSTRING:
					sc.SetState(SCE_TITLEFORMAT_DEFAULT);
					break;
				case SCE_TITLEFORMAT_FIELD:
					if (sc.ch == '%') {
						sc.ForwardSetState(SCE_TITLEFORMAT_DEFAULT);
					}
					break;
				}

				// Determine if a new state should be entered.
				if (sc.state == SCE_TITLEFORMAT_DEFAULT) {
					inactiveFlag = 0;

					currentRange = privateCall->GetInactiveRangeLowerBound(sc.currentPos);

					if ((currentRange != rangeCount) &&
						privateCall->InactiveRangeContainsCharacter(currentRange, sc.currentPos)) {
						inactiveFlag = 64;
					}

					if (sc.Match('/', '/')) {
						sc.SetState(SCE_TITLEFORMAT_COMMENT);
						sc.Forward();
					}
					else if (IsTitleFormatOperator(sc.ch)) {
						sc.SetState(SCE_TITLEFORMAT_OPERATOR | inactiveFlag);
					}
					else if (IsASpecialStringChar(sc.ch) && sc.ch == sc.chNext) {
						sc.SetState(SCE_TITLEFORMAT_SPECIALSTRING | inactiveFlag);
						sc.Forward();
					}
					else if (sc.ch == '$') {
						sc.SetState(SCE_TITLEFORMAT_IDENTIFIER | inactiveFlag);
					}
					else if (sc.ch == '%') {
						sc.SetState(SCE_TITLEFORMAT_FIELD | inactiveFlag);
					}
					else if (sc.ch == '\'') {
						sc.SetState(SCE_TITLEFORMAT_STRING | inactiveFlag);
					}
					else if (IsALiteralStringChar(sc.ch)) {
						sc.SetState(SCE_TITLEFORMAT_LITERALSTRING | inactiveFlag);
					}
				}
			}
			sc.Complete();
		}

		void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, IDocument* pAccess) {

			if (!options.fold)
				return;

			Accessor styler(pAccess, nullptr);

			int startLine = styler.GetLine(startPos);
			int levelPrev = styler.LevelAt(startLine) & SC_FOLDLEVELNUMBERMASK;
			bool commentPrev = false;
			if (startLine > 0) {
				commentPrev = styler.Match(styler.LineStart(startLine - 1), "//");
			}

			int lastLine = styler.GetLine(lengthDoc);

			for (int line = startLine; line < lastLine; ++line) {
				int level = SC_FOLDLEVELBASE;

				bool comment = styler.Match(styler.LineStart(line), "//");
				if (comment && commentPrev) {
					level += 1;
				}

				int levelAndFlags = level;

				if (comment && !commentPrev) {
					levelAndFlags |= SC_FOLDLEVELHEADERFLAG;
				}

				styler.SetLevel(line, levelAndFlags);

				levelPrev = level;
				commentPrev = comment;
			}
		}

		void* SCI_METHOD PrivateCall(int operation, void* pointer) override {
			if (operation == 1/*SPC_TITLEFORMAT_GETINTERFACE*/ && pointer == 0) {
				privateCall->AddRef();
				return privateCall;
			}
			return 0;
		}

		void BacktrackToStart(const LexAccessor& styler, int stateMask, Sci_PositionU& startPos, Sci_Position& lengthDoc, int& initStyle) {
				const Sci_Position currentLine = styler.GetLine(startPos);
				if (currentLine != 0) {
					Sci_Position line = currentLine - 1;
					int lineState = styler.GetLineState(line);
					while ((lineState & stateMask) != 0 && line != 0) {
						--line;
						lineState = styler.GetLineState(line);
					}
					if ((lineState & stateMask) == 0) {
						++line;
					}
					if (line != currentLine) {
						const Sci_PositionU endPos = startPos + lengthDoc;
						startPos = (line == 0) ? 0 : styler.LineStart(line);
						lengthDoc = endPos - startPos;
						initStyle = (startPos == 0) ? 0 : styler.StyleAt(startPos - 1);
					}
				}
		}

		Sci_PositionU LookbackNonWhite(LexAccessor& styler, Sci_PositionU startPos, int& chPrevNonWhite, int& stylePrevNonWhite);

		static ILexer5* LexerFactoryTitleFormat() {
			return new LexerTitleFormat("titleformat", SCLEX_TITLEFORMAT);
		}

	private:

		LexerTitleformatPrivateCall* privateCall = nullptr;	
	};

}  // unnamed namespace end

extern const LexerModule lmTitleFormat(SCLEX_TITLEFORMAT, LexerTitleFormat::LexerFactoryTitleFormat, "titleformat", wordListsDesc);
