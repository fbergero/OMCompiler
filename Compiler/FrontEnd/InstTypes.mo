/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Linköping University,
 * Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3 
 * AND THIS OSMC PUBLIC LICENSE (OSMC-PL). 
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES RECIPIENT'S  
 * ACCEPTANCE OF THE OSMC PUBLIC LICENSE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from Linköping University, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or  
 * http://www.openmodelica.org, and in the OpenModelica distribution. 
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS
 * OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

encapsulated package InstTypes
" file:        InstTypes.mo
  package:     InstTypes
  description: Types used by SCodeInst.

  RCS: $Id$

  Types used by SCodeInst.
"

public import Absyn;
public import Connect;
public import DAE;
public import SCode;
public import SCodeEnv;

public type Prefix = list<tuple<String, DAE.Dimensions>>;
public constant Prefix emptyPrefix = {};

public uniontype Element
  record ELEMENT
    Component component;
    Class cls;
  end ELEMENT;

  record CONDITIONAL_ELEMENT
    Component component;
  end CONDITIONAL_ELEMENT;

  record EXTENDED_ELEMENTS
    Absyn.Path baseClass;
    Class cls;
    DAE.Type ty;
  end EXTENDED_ELEMENTS;
end Element;

public uniontype Class
  record COMPLEX_CLASS
    list<Element> components;
    list<Equation> equations;
    list<Equation> initialEquations;
    list<SCode.AlgorithmSection> algorithms;
    list<SCode.AlgorithmSection> initialAlgorithms;
  end COMPLEX_CLASS;

  record BASIC_TYPE end BASIC_TYPE;
end Class;

public uniontype Dimension
  record UNTYPED_DIMENSION
    DAE.Dimension dimension;
    Boolean isProcessing;
  end UNTYPED_DIMENSION;

  record TYPED_DIMENSION
    DAE.Dimension dimension;
  end TYPED_DIMENSION;
end Dimension;

public uniontype Binding
  record UNBOUND end UNBOUND;

  record RAW_BINDING
    Absyn.Exp bindingExp;
    SCodeEnv.Env env;
    Prefix prefix;
    Integer propagatedDims "See SCodeMod.propagateMod.";
    Absyn.Info info;
  end RAW_BINDING;

  record UNTYPED_BINDING
    DAE.Exp bindingExp;
    Boolean isProcessing;
    Integer propagatedDims "See SCodeMod.propagateMod.";
    Absyn.Info info;
  end UNTYPED_BINDING;

  record TYPED_BINDING
    DAE.Exp bindingExp;
    DAE.Type bindingType;
    Integer propagatedDims "See SCodeMod.propagateMod.";
    Absyn.Info info;
  end TYPED_BINDING;
end Binding;

public uniontype Component
  record UNTYPED_COMPONENT
    Absyn.Path name;
    DAE.Type baseType;
    array<Dimension> dimensions;
    Prefixes prefixes;
    ParamType paramType;
    Binding binding;
    Absyn.Info info;
  end UNTYPED_COMPONENT;

  record TYPED_COMPONENT
    Absyn.Path name;
    DAE.Type ty;
    Prefixes prefixes;
    Binding binding;
    Absyn.Info info;
  end TYPED_COMPONENT;
    
  record CONDITIONAL_COMPONENT
    Absyn.Path name;
    DAE.Exp condition;
    SCode.Element element;
    Modifier modifier;
    Prefixes prefixes;
    SCodeEnv.Env env;
    Prefix prefix;
    Absyn.Info info;
  end CONDITIONAL_COMPONENT; 

  record OUTER_COMPONENT
    Absyn.Path name;
    Option<Absyn.Path> innerName;
  end OUTER_COMPONENT;

  record PACKAGE
    Absyn.Path name;
  end PACKAGE;
end Component;

public uniontype ParamType
  record NON_PARAM "Not a parameter." end NON_PARAM;
  record NON_STRUCT_PARAM "A non-structural parameter." end NON_STRUCT_PARAM;
  record STRUCT_PARAM "A structural parameter." end STRUCT_PARAM;
end ParamType;

public uniontype Modifier
  record MODIFIER
    String name;
    SCode.Final finalPrefix;
    SCode.Each eachPrefix;
    Binding binding;
    list<Modifier> subModifiers;
    Absyn.Info info;
  end MODIFIER;

  record REDECLARE
    SCode.Final finalPrefix;
    SCode.Each eachPrefix;
    SCode.Element element;
  end REDECLARE;

  record NOMOD end NOMOD;
end Modifier;

public uniontype Prefixes
  record NO_PREFIXES end NO_PREFIXES;

  record PREFIXES
    DAE.VarVisibility visibility;
    DAE.VarKind variability;
    SCode.Final finalPrefix;
    Absyn.InnerOuter innerOuter;
    tuple<DAE.VarDirection, Absyn.Info> direction;
    tuple<DAE.Flow, Absyn.Info> flowPrefix;
    tuple<DAE.Stream, Absyn.Info> streamPrefix;
  end PREFIXES;
end Prefixes;

public constant Prefixes DEFAULT_CONST_PREFIXES = PREFIXES(
  DAE.PUBLIC(), DAE.CONST(), SCode.NOT_FINAL(), Absyn.NOT_INNER_OUTER(),
  (DAE.BIDIR(), Absyn.dummyInfo), (DAE.NON_CONNECTOR(), Absyn.dummyInfo),
  (DAE.NON_STREAM_CONNECTOR(), Absyn.dummyInfo));

public uniontype Equation
  record EQUALITY_EQUATION
    DAE.Exp lhs "The left hand side expression.";
    DAE.Exp rhs "The right hand side expression.";
    Absyn.Info info;
  end EQUALITY_EQUATION;

  record CONNECT_EQUATION
    DAE.ComponentRef lhs "The left hand side component.";
    Connect.Face lhsFace "The face of the lhs component, inside or outside.";
    DAE.ComponentRef rhs "The right hand side component.";
    Connect.Face rhsFace "The face of the rhs component, inside or outside.";
    Prefix prefix;
    Absyn.Info info;
  end CONNECT_EQUATION;

  record FOR_EQUATION
    String index "The name of the index/iterator variable.";
    DAE.Type indexType "The type of the index/iterator variable.";
    DAE.Exp range "The range expression to loop over.";
    list<Equation> body "The body of the for loop.";
    Absyn.Info info;
  end FOR_EQUATION;

  record IF_EQUATION
    list<tuple<DAE.Exp, list<Equation>>> branches
      "List of branches, where each branch is a tuple of a condition and a body.";
    Absyn.Info info;
  end IF_EQUATION;

  record WHEN_EQUATION
    list<tuple<DAE.Exp, list<Equation>>> branches
      "List of branches, where each branch is a tuple of a condition and a body.";
    Absyn.Info info;
  end WHEN_EQUATION;

  record ASSERT_EQUATION
    DAE.Exp condition "The assert condition.";
    DAE.Exp message "The message to display if the assert fails.";
    Absyn.Info info;
  end ASSERT_EQUATION;

  record TERMINATE_EQUATION
    DAE.Exp message "The message to display if the terminate triggers.";
    Absyn.Info info;
  end TERMINATE_EQUATION;

  record REINIT_EQUATION
    DAE.ComponentRef cref "The variable to reinitialize.";
    DAE.Exp reinitExp "The new value of the variable.";
    Absyn.Info info;
  end REINIT_EQUATION;
end Equation;

end InstTypes;
