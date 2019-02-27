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

#include "SynTreeGen.h"
#include "EbnfSyntax.h"
#include "EbnfAnalyzer.h"
#include "GenUtils.h"
#include <QDir>
#include <QTextStream>

bool SynTreeGen::generateTree(const QString& ebnfPath, EbnfSyntax* syn, const QByteArray& nameSpace, bool includeNt)
{
    QDir dir = QFileInfo(ebnfPath).dir();

    QFile header( dir.absoluteFilePath( nameSpace + "SynTree.h") );
    header.open( QIODevice::WriteOnly );
    QTextStream hout(&header);
    hout.setCodec("Latin-1");

    const QByteArray stopLabel = "__" + nameSpace.toUpper() + ( !nameSpace.isEmpty() ? "_" : "" ) + "SYNTREE__";
    hout << "#ifndef " << stopLabel << endl;
    hout << "#define " << stopLabel << endl;
    hout << "// This file was automatically generated by EbnfStudio; don't modify it!" << endl;
    hout << endl;
    hout << "#include <" << nameSpace << "TokenType.h>" << endl;
    hout << "#include <" << nameSpace << "Token.h>" << endl;
    hout << "#include <QList>" << endl;
    hout << endl;

    if( !nameSpace.isEmpty() )
        hout << "namespace " << nameSpace << " {" << endl;

    hout << endl;
    hout << "\t" << "struct SynTree {" << endl;

    typedef QMap<QString,const EbnfSyntax::Definition*> DefSort;
    DefSort sort;
    foreach( const EbnfSyntax::Definition* d, syn->getDefs() )
    {
        if( d->d_tok.d_op != EbnfToken::Transparent && !d->d_usedBy.isEmpty() )
            sort.insert( GenUtils::escapeDollars( d->d_tok.d_val ), d );
    }

    DefSort::const_iterator i;
    if( includeNt )
    {
        hout << "\t\t" << "enum ParserRule {" << endl;
        hout << "\t\t\t" << "R_First = TT_Max + 1," << endl;
        for( i = sort.begin(); i != sort.end(); ++i )
        {
            hout << "\t\t\t" << "R_" << i.key() << "," << endl;
        }
        hout << "\t\t\t" << "R_Last" << endl;
        hout << "\t\t" << "};" << endl;
    }

    hout << "\t\t" << "SynTree(quint16 r = Tok_Invalid, const Token& = Token() );" << endl;
    hout << "\t\t" << "SynTree(const Token& t ):d_tok(t){}" << endl;
    hout << "\t\t" << /* "virtual " << */ "~SynTree() { foreach(SynTree* n, d_children) delete n; }" << endl;
    hout << endl;
    hout << "\t\t" << "static const char* rToStr( quint16 r );" << endl;
    hout << endl;
    hout << "\t\t" << "Vl::Token d_tok;" << endl;
    hout << "\t\t" << "QList<SynTree*> d_children;" << endl;
    hout << "\t" << "};" << endl;
    hout << endl;
    if( !nameSpace.isEmpty() )
        hout << "}" << endl;
    hout << "#endif // " << stopLabel << endl;

    QFile body( dir.absoluteFilePath( nameSpace + "SynTree.cpp") );
    body.open( QIODevice::WriteOnly );
    QTextStream bout(&body);
    bout.setCodec("Latin-1");

    bout << "// This file was automatically generated by VerilogEbnf; don't modify it!" << endl;
    bout << "#include \"" << nameSpace << "SynTree.h\"" << endl;
    if( !nameSpace.isEmpty() )
        bout << "using namespace Vl;" << endl;
    bout << endl;

    bout << "SynTree::SynTree(quint16 r, const Token& t ):d_tok(r){" << endl;
    bout << "\t" << "d_tok.d_lineNr = t.d_lineNr;" << endl;
    bout << "\t" << "d_tok.d_colNr = t.d_colNr;" << endl;
    bout << "\t" << "d_tok.d_sourcePath = t.d_sourcePath;" << endl;
    // don't copy: bout << "\t" << "d_tok.d_val = t.d_val;" << endl;
    bout << "}" << endl;
    bout << endl;

    bout << "const char* SynTree::rToStr( quint16 r ) {" << endl;
    if( includeNt )
    {
        bout << "\t" << "switch(r) {" << endl;
        for( i = sort.begin(); i != sort.end(); ++i )
        {
            bout << "\t\tcase R_" << i.key()
                << ": return \"" << i.value()->d_tok.d_val.toStr() << "\";" << endl;
        }
        bout << "\tdefault: if(r<R_First) return tokenTypeName(r); else return \"\";" << endl;
        bout << "}" << endl;
    }else
        bout << "\t\t" << "return tokenTypeName(r);" << endl;
    bout << "}" << endl;

    return true;
}


SynTreeGen::SynTreeGen()
{

}

SynTreeGen::TokenNameValueList SynTreeGen::generateTokenList(EbnfSyntax* syn)
{
    TokenNameValueList res;

    const QByteArrayList tokens = GenUtils::orderedTokenList( EbnfAnalyzer::collectAllTerminalStrings( syn ), false );
    const QByteArrayList specials = EbnfAnalyzer::collectAllTerminalProductions( syn );

    // Tok_Invalid ist hier absichtlich nicht enthalten, da z.B. Coco/R Index=0 für _EOF beansprucht

    res.append( qMakePair(QByteArray("Literals"),QByteArray()) );

    bool keyWordSection = false;
    for( int i = 0; i < tokens.size(); i++ )
    {
        if( !keyWordSection && GenUtils::containsAlnum( tokens[i] ) )
        {
            res.append( qMakePair(QByteArray("Keywords"),QByteArray()) );
            keyWordSection = true;
        }
        res.append( qMakePair(GenUtils::symToString(tokens[i]),tokens[i]) );
    }
    res.append( qMakePair(QByteArray("Specials"),QByteArray()) );

    foreach( const QByteArray& t, specials )
        res.append( qMakePair(GenUtils::escapeDollars(t),t) );

    res.append( qMakePair(QByteArray("Eof"),QByteArray("<eof>") ) );
    res.append( qMakePair(QByteArray("MaxToken"),QByteArray()) );

    return res;
}

bool SynTreeGen::generateTt(const QString& ebnfPath, EbnfSyntax* syn, const QByteArray& nameSpace, bool includeNt )
{
    QDir dir = QFileInfo(ebnfPath).dir();

    QFile header( dir.absoluteFilePath( nameSpace + "TokenType.h") );
    header.open( QIODevice::WriteOnly );
    QTextStream hout(&header);
    hout.setCodec("Latin-1");

    const QByteArray stopLabel = "__" + nameSpace.toUpper() + ( !nameSpace.isEmpty() ? "_" : "" ) + "TOKENTYPE__";
    hout << "#ifndef " << stopLabel << endl;
    hout << "#define " << stopLabel << endl;
    hout << "// This file was automatically generated by EbnfStudio; don't modify it!" << endl;
    hout << endl;

    if( !nameSpace.isEmpty() )
        hout << "namespace " << nameSpace << " {" << endl;

    TokenNameValueList tokens = generateTokenList(syn);

    hout << "\t" << "enum TokenType {" << endl;
    hout << "\t\t" << "Tok_Invalid = 0," << endl;

    for( int i = 0; i < tokens.size(); i++ )
    {
        if( tokens[i].second.isEmpty() )
            hout << endl << "\t\t" << "TT_" << tokens[i].first << "," << endl;
        else
            hout << "\t\t" << "Tok_" << tokens[i].first << "," << endl;
    }
    if( includeNt )
    {
        hout << endl << "\t\t" << "TT_Nonterminals," << endl;

        foreach( const EbnfSyntax::Definition* d, syn->getDefs() )
        {
            if( d->d_tok.d_op == EbnfToken::Normal && !d->d_usedBy.isEmpty() && d->d_node != 0 )
                hout << "\t\t" << "R_" << GenUtils::escapeDollars( d->d_tok.d_val ) << "," << endl;
        }
    }
    hout << endl << "\t\t" << "TT_Max" << endl;
    hout << "\t" << "};" << endl;

    hout << endl;

    hout << "\t" << "const char* tokenTypeString( int ); // Pretty with punctuation chars" << endl;
    hout << "\t" << "const char* tokenTypeName( int ); // Just the names without punctuation chars" << endl;
    hout << "\t" << "bool tokenTypeIsLiteral( int );" << endl;
    hout << "\t" << "bool tokenTypeIsKeyword( int );" << endl;
    hout << "\t" << "bool tokenTypeIsSpecial( int );" << endl;
    if( includeNt )
        hout << "\t" << "bool tokenTypeIsNonterminal( int );" << endl;

    if( !nameSpace.isEmpty() )
        hout << "}" << endl; // end namespace

    hout << "#endif // " << stopLabel << endl;

    QFile body( dir.absoluteFilePath( nameSpace + "TokenType.cpp") );
    body.open( QIODevice::WriteOnly );
    QTextStream bout(&body);
    bout.setCodec("Latin-1");

    bout << "// This file was automatically generated by EbnfStudio; don't modify it!" << endl;
    bout << "#include \"" << nameSpace << "TokenType.h\"" << endl;
    bout << endl;

    if( !nameSpace.isEmpty() )
        bout << "namespace " << nameSpace << " {"<< endl;

    bout << "\t" << "const char* tokenTypeString( int r ) {" << endl;
    bout << "\t\t" << "switch(r) {" << endl;
    bout << "\t\t\t" << "case Tok_Invalid: return \"<invalid>\";" << endl;

    for( int i = 0; i < tokens.size(); i++ )
    {
        if( !tokens[i].second.isEmpty() )
            bout << "\t\t\t" << "case Tok_" << tokens[i].first <<
                ": return \"" << tokens[i].second << "\";" << endl;
    }
    if( includeNt )
    {
        foreach( const EbnfSyntax::Definition* d, syn->getDefs() )
        {
            if( d->d_tok.d_op == EbnfToken::Normal && !d->d_usedBy.isEmpty() && d->d_node != 0 )
                bout << "\t\t\t" << "case R_" << GenUtils::escapeDollars( d->d_tok.d_val ) <<
                        ": return \"" << d->d_tok.d_val.toStr() << "\";" << endl;
        }
    }

    bout << "\t\t\t" << "default: return \"\";" << endl;
    bout << "\t\t" << "}" << endl; // switch
    bout << "\t" << "}" << endl; // function

    bout << "\t" << "const char* tokenTypeName( int r ) {" << endl;
    bout << "\t\t" << "switch(r) {" << endl;
    bout << "\t\t\t" << "case Tok_Invalid: return \"Tok_Invalid\";" << endl;

    for( int i = 0; i < tokens.size(); i++ )
    {
        if( !tokens[i].second.isEmpty() )
            bout << "\t\t\t" << "case Tok_" << tokens[i].first <<
                ": return \"Tok_" << tokens[i].first << "\";" << endl;
    }
    if( includeNt )
    {
        foreach( const EbnfSyntax::Definition* d, syn->getDefs() )
        {
            const QByteArray name = GenUtils::escapeDollars( d->d_tok.d_val );
            if( d->d_tok.d_op == EbnfToken::Normal && !d->d_usedBy.isEmpty() && d->d_node != 0 )
                bout << "\t\t\t" << "case R_" << name <<
                        ": return \"R_" << name << "\";" << endl;
        }
    }

    bout << "\t\t\t" << "default: return \"\";" << endl;
    bout << "\t\t" << "}" << endl; // switch
    bout << "\t" << "}" << endl; // function


    bout << "\t" << "bool tokenTypeIsLiteral( int r ) {" << endl;
    bout << "\t\t" << "return r > TT_Literals && r < TT_Keywords;" << endl;
    bout << "\t" << "}" << endl; // function

    bout << "\t" << "bool tokenTypeIsKeyword( int r ) {" << endl;
    bout << "\t\t" << "return r > TT_Keywords && r < TT_Specials;" << endl;
    bout << "\t" << "}" << endl; // function

    bout << "\t" << "bool tokenTypeIsSpecial( int r ) {" << endl;
    if( includeNt )
        bout << "\t\t" << "return r > TT_Specials && r < TT_Nonterminals;" << endl;
    else
        bout << "\t\t" << "return r > TT_Specials && r < TT_Max;" << endl;
    bout << "\t" << "}" << endl; // function

    if( includeNt )
    {
        bout << "\t" << "bool tokenTypeIsNonterminal( int r ) {" << endl;
        bout << "\t\t" << "return r > TT_Nonterminals && r < TT_Max;" << endl;
        bout << "\t" << "}" << endl; // function
    }

    if( !nameSpace.isEmpty() )
        bout << "}" << endl; // namespace
    return true;
}

