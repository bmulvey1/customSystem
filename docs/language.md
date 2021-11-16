# The Language
## overview

literally just the b programming language
https://www.bell-labs.com/usr/dmr/www/kbman.pdf


Updated Syntax
I'm about the furthest from a parser designer
But trying to parse this with even 2 tokens of lookahead isn't fun
Here's my attempt to rewrite the grammar so it can be parsed with only 1 token of LA

	program ::=
    	definition_list

	definition_list ::=
		definition definition_list
		ε

    definition ::=
		name [ optional_constant ] optional_ival_list;
		name optional_ival_list;
		name ( optional_name_list ) statement;

    ival ::=
    	constant
    	name

    statement ::=
    	auto name {constant}01 {, name {constant}01}0 ;  statement
    	extrn name {, name}0 ; statement
    	name : statement
    	case constant : statement
    	{ {statement}0 }
    	if ( rvalue ) statement {else statement}01
    	while ( rvalue ) statement
    	switch rvalue statement
    	goto rvalue ;
    	return {( rvalue )}01 ;
    	{rvalue}01 ;

    rvalue ::=
    	( rvalue )
    	lvalue
    	constant
    	lvalue assign rvalue
    	inc-dec lvalue
    	lvalue inc-dec
    	unary rvalue
    	& lvalue
    	rvalue binary rvalue
    	rvalue ? rvalue : rvalue
    	rvalue ( {rvalue {, rvalue}0 }01 )

    assign ::=
    	= {binary}01

    inc-dec ::=
    	++
    	--
    	
    unary ::=
    	-
    	!

    binary ::=
    	|
    	&
    	==
    	!=
    	<
    	<=
    	>
    	>=
    	<<
    	>>
    	-
    	+
    	%
    	*
    	/

    lvalue ::=
    	name
    	* rvalue
    	rvalue [ rvalue ]

    constant ::=
    	{digit}1
    	' {char}12 '
    	" {char}0 "

    name ::=
    	alpha {alpha-digit}07

    alpha-digit ::=
    	alpha
    	digit



Canonical Syntax


The syntax given in this section defines all the legal constructions in B without specifying the association rules. These are given later along with the semantic description of each construction.

    program ::=
    	{definition}0

    definition ::=
    	name {[ {constant}01 ]}01 {ival {, ival}0}01 ;
    	name ( {name {, name}0}01 ) statement

    ival ::=
    	constant
    	name

    statement ::=
    	auto name {constant}01 {, name {constant}01}0 ;  statement
    	extrn name {, name}0 ; statement
    	name : statement
    	case constant : statement
    	{ {statement}0 }
    	if ( rvalue ) statement {else statement}01
    	while ( rvalue ) statement
    	switch rvalue statement
    	goto rvalue ;
    	return {( rvalue )}01 ;
    	{rvalue}01 ;

    rvalue ::=
    	( rvalue )
    	lvalue
    	constant
    	lvalue assign rvalue
    	inc-dec lvalue
    	lvalue inc-dec
    	unary rvalue
    	& lvalue
    	rvalue binary rvalue
    	rvalue ? rvalue : rvalue
    	rvalue ( {rvalue {, rvalue}0 }01 )

    assign ::=
    	= {binary}01

    inc-dec ::=
    	++
    	--
    	
    unary ::=
    	-
    	!

    binary ::=
    	|
    	&
    	==
    	!=
    	<
    	<=
    	>
    	>=
    	<<
    	>>
    	-
    	+
    	%
    	*
    	/

    lvalue ::=
    	name
    	* rvalue
    	rvalue [ rvalue ]

    constant ::=
    	{digit}1
    	' {char}12 '
    	" {char}0 "

    name ::=
    	alpha {alpha-digit}07

    alpha-digit ::=
    	alpha
    	digit
