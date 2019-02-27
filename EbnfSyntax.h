#ifndef EBNF_SYNTAX_H
#define EBNF_SYNTAX_H

/*
* Copyright 2019 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the EbnfStudio application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QSharedData>
#include <QHash>
#include <QStringList>
#include <QSet>
#include <QVariant>
#include "EbnfToken.h"

class EbnfErrors;

class EbnfSyntax : public QSharedData
{
public:
    struct Node;
    typedef QList<Node*> NodeList;
    typedef QList<const Node*> ConstNodeList;

    struct Symbol
    {
        EbnfToken d_tok;
        Symbol( const EbnfToken& tok = EbnfToken() ):d_tok(tok) {}
        virtual bool doIgnore() const { return false; }
        virtual bool isNullable() const { return false; }
        virtual bool isRepeatable() const { return false; }
    };

    struct Definition : public Symbol
    {
        Node* d_node;
        QSet<Node*> d_usedBy; // TODO: könnte auch List sein!
        bool d_nullable;
        bool d_repeatable;
        bool d_directLeftRecursive;
        bool d_indirectLeftRecursive;
        Definition(const EbnfToken& tok):Symbol(tok),d_node(0),d_nullable(false),d_repeatable(false),
            d_directLeftRecursive(false),d_indirectLeftRecursive(false){}
        ~Definition() { if( d_node ) delete d_node; }
        bool doIgnore() const;
        bool isNullable() const { return d_nullable; }
        bool isRepeatable() const { return d_repeatable; }
        void dump() const;
    };

    struct Node : public Symbol
    {
        enum Type { Terminal, Nonterminal, Sequence, Alternative, Predicate };
        enum Quantity { One, ZeroOrOne, ZeroOrMore };
        static const char* s_typeName[];
#ifdef _DEBUG
        Type d_type;
        Quantity d_quant;
#else
        quint8 d_type;
        quint8 d_quant;
#endif
        bool d_leftRecursive;
        NodeList d_subs; // owned
        NodeList d_pathToDef;
        Definition* d_owner;
        Definition* d_def; // resolved nonterminal
        Node* d_parent; // TODO: ev. unnötig; man kann damit bottom up über Sequence hinweg schauen
        Node(Type t, Definition* d, const EbnfToken& tok = EbnfToken()):Symbol(tok),d_type(t),
            d_quant(One),d_owner(d),d_def(0),d_parent(0),d_leftRecursive(false){}
        Node(Type t, Node* parent, const EbnfToken& tok = EbnfToken()):Symbol(tok),d_type(t),
            d_quant(One),d_owner(parent->d_owner),d_def(0),d_parent(parent),d_leftRecursive(false){ parent->d_subs.append(this); }
        ~Node();
        bool doIgnore() const;
        bool isNullable() const;
        bool isRepeatable() const;
        const Node* getNext(int* index = 0) const;
        int getLlk() const; // 0..invalid
        void dump(int level = 0) const;
    };
    typedef QSet<Definition*> DefSet;

    struct NodeRef
    {
        const EbnfSyntax::Node* d_node;
        NodeRef( const EbnfSyntax::Node* node = 0 ):d_node(node){}
        bool operator==( const NodeRef& ) const;
        const EbnfSyntax::Node* operator*() const { return d_node; }
    };
    typedef QSet<NodeRef> NodeSet;
    typedef QList<NodeRef> NodeRefList;
    static QString pretty( const NodeSet& );

    struct IssueData
    {
        enum Type { None, AmbigAlt, AmbigOpt, BadPred, LeftRec } d_type;
        const Node* d_ref;
        const Node* d_other;
        NodeRefList d_list;
        IssueData(Type t = None, const Node* r = 0, const Node* oth = 0, const NodeRefList& l = NodeRefList()):
            d_type(t), d_ref(r),d_other(oth),d_list(l){}
    };

    explicit EbnfSyntax(EbnfErrors* errs = 0);
    ~EbnfSyntax();

    void clear();

    typedef QHash<QByteArray,Definition*> Definitions;
    typedef QList<Definition*> OrderedDefs;

    bool addDef( Definition* ); // transfer ownership
    const Definitions& getDefs() const { return d_defs; }
    const Definition* getDef(const QByteArray& name ) const;
    const OrderedDefs& getOrderedDefs() const { return d_order; }

    bool finishSyntax();

    const Symbol* findSymbolBySourcePos( quint32 line, quint16 col , bool nonTermOnly = true ) const;
    ConstNodeList getBackRefs( const Symbol* ) const;
    static const Node* firstVisibleElementOf( const Node* );
    static const Node* firstPredicateOf( const Node* );

    void dump() const;
protected:
    bool resolveAllSymbols();
    void calculateNullable();
    bool resolveAllSymbols( Node *node );
    const Symbol* findSymbolBySourcePosImp( const Node*, quint32 line, quint16 col, bool nonTermOnly ) const;
    void calcLeftRecursion();
    void markLeftRecursion( Definition*,Node* node, NodeList& );
    NodeSet calcStartsWithNtSet( Node* node );
private:
    Q_DISABLE_COPY(EbnfSyntax)
    EbnfErrors* d_errs;
    Definitions d_defs;
    OrderedDefs d_order;
    typedef QHash<EbnfToken::Sym,ConstNodeList> BackRefs;
    BackRefs d_backRefs;
    bool d_finished;
};

typedef QExplicitlySharedDataPointer<EbnfSyntax> EbnfSyntaxRef;
//inline uint qHash(const EbnfSyntax::NodeRef& r ) { return r.d_node ? qHash(r.d_node->d_tok.d_val) : 0; }
// NOTE: weil alle Symbole denselben String verwenden auf Adresse als Hash wechseln
inline uint qHash(const EbnfSyntax::NodeRef& r ) { return r.d_node ? qHash(r.d_node->d_tok.d_val) : 0; }

Q_DECLARE_METATYPE(EbnfSyntax::IssueData)

#endif // EBNF_SYNTAX_H