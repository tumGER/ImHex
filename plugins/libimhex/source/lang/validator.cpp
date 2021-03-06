#include <hex/lang/validator.hpp>

#include <unordered_set>
#include <string>

#include <hex/helpers/utils.hpp>

namespace hex::lang {

    Validator::Validator() {

    }

    bool Validator::validate(const std::vector<ASTNode*>& ast) {
        std::unordered_set<std::string> identifiers;

        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    throwValidateError("nullptr in AST. This is a bug!", 1);

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName().data()).second)
                        throwValidateError(hex::format("redefinition of identifier '{0}'", variableDeclNode->getName().data()), variableDeclNode->getLineNumber());

                    this->validate({ variableDeclNode->getType() });
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                    if (!identifiers.insert(typeDeclNode->getName().data()).second)
                        throwValidateError(hex::format("redefinition of identifier '{0}'", typeDeclNode->getName().data()), typeDeclNode->getLineNumber());

                    this->validate({ typeDeclNode->getType() });
                } else if (auto structNode = dynamic_cast<ASTNodeStruct*>(node); structNode != nullptr) {
                    this->validate(structNode->getMembers());
                } else if (auto unionNode = dynamic_cast<ASTNodeUnion*>(node); unionNode != nullptr) {
                    this->validate(unionNode->getMembers());
                } else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(node); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            throwValidateError(hex::format("redefinition of enum constant '{0}'", name.c_str()), value->getLineNumber());
                    }
                }
            }

        } catch (ValidatorError &e) {
            this->m_error = e;
            return false;
        }

        return true;
    }

    void Validator::printAST(const std::vector<ASTNode*>& ast){
    #if DEBUG
        #define INDENT_VALUE indent, ' '
        static s32 indent = -2;

        indent += 2;
        for (const auto &node : ast) {
            if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(variableDeclNode->getPlacementOffset()); offset != nullptr) {
                    printf("%*c ASTNodeVariableDecl (%s) @\n", INDENT_VALUE, variableDeclNode->getName().data());
                    printAST({ offset });
                }
                else
                    printf("%*c ASTNodeVariableDecl (%s)\n", INDENT_VALUE, variableDeclNode->getName().data());
                printAST({ variableDeclNode->getType() });
            } else if (auto pointerDeclNode = dynamic_cast<ASTNodePointerVariableDecl*>(node); pointerDeclNode != nullptr) {
                if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(pointerDeclNode->getPlacementOffset()); offset != nullptr) {
                    printf("%*c ASTNodePointerVariableDecl (*%s) @\n", INDENT_VALUE, pointerDeclNode->getName().data());
                    printAST({ offset });
                }
                else
                    printf("%*c ASTNodePointerVariableDecl (*%s)\n", INDENT_VALUE, pointerDeclNode->getName().data());
                printAST({ pointerDeclNode->getType() });
                printAST({ pointerDeclNode->getSizeType() });
            } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); arrayDeclNode != nullptr) {
                auto sizeExpr = dynamic_cast<ASTNodeNumericExpression*>(arrayDeclNode->getSize());
                if (sizeExpr == nullptr) {
                    printf("%*c Invalid size!\n", INDENT_VALUE);
                    continue;
                }

                if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(arrayDeclNode->getPlacementOffset()); offset != nullptr) {
                    printf("%*c ASTNodeArrayVariableDecl (%s[]) @\n", INDENT_VALUE, arrayDeclNode->getName().data());
                    printAST({ sizeExpr });
                    printAST({ offset });
                }
                else {
                    printf("%*c ASTNodeArrayVariableDecl (%s[])\n", INDENT_VALUE, arrayDeclNode->getName().data());
                    printAST({ sizeExpr });
                }

                printAST({ arrayDeclNode->getType() });
                printAST({ arrayDeclNode->getSize() });
            } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                printf("%*c ASTNodeTypeDecl (%s %s)\n", INDENT_VALUE, typeDeclNode->getEndian().value_or(std::endian::native) == std::endian::little ? "le" : "be", typeDeclNode->getName().empty() ? "<unnamed>" : typeDeclNode->getName().data());
                printAST({ typeDeclNode->getType() });
            } else if (auto builtinTypeNode = dynamic_cast<ASTNodeBuiltinType*>(node); builtinTypeNode != nullptr) {
                std::string typeName = Token::getTypeName(builtinTypeNode->getType());
                printf("%*c ASTNodeTypeDecl (%s)\n", INDENT_VALUE, typeName.c_str());
            } else if (auto integerLiteralNode = dynamic_cast<ASTNodeIntegerLiteral*>(node); integerLiteralNode != nullptr) {
                printf("%*c ASTNodeIntegerLiteral %lld\n", INDENT_VALUE, (s64)std::get<s128>(integerLiteralNode->getValue()));
            } else if (auto numericExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(node); numericExpressionNode != nullptr) {
                std::string op;
                switch (numericExpressionNode->getOperator()) {
                    case Token::Operator::Plus: op = "+"; break;
                    case Token::Operator::Minus: op = "-"; break;
                    case Token::Operator::Star: op = "*"; break;
                    case Token::Operator::Slash: op = "/"; break;

                    case Token::Operator::ShiftLeft: op = ">>"; break;
                    case Token::Operator::ShiftRight: op = "<<"; break;

                    case Token::Operator::BitAnd: op = "&"; break;
                    case Token::Operator::BitOr: op = "|"; break;
                    case Token::Operator::BitXor: op = "^"; break;
                    default: op = "???";
                }
                printf("%*c ASTNodeNumericExpression %s\n", INDENT_VALUE, op.c_str());
                printf("%*c Left:\n", INDENT_VALUE);
                printAST({ numericExpressionNode->getLeftOperand() });
                printf("%*c Right:\n", INDENT_VALUE);
                printAST({ numericExpressionNode->getRightOperand() });
            } else if (auto structNode = dynamic_cast<ASTNodeStruct*>(node); structNode != nullptr) {
                printf("%*c ASTNodeStruct\n", INDENT_VALUE);
                printAST(structNode->getMembers());
            } else if (auto unionNode = dynamic_cast<ASTNodeUnion*>(node); unionNode != nullptr) {
                printf("%*c ASTNodeUnion\n", INDENT_VALUE);
                printAST(unionNode->getMembers());
            } else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(node); enumNode != nullptr) {
                printf("%*c ASTNodeEnum\n", INDENT_VALUE);

                for (const auto &[name, entry] : enumNode->getEntries()) {
                    printf("%*c ::%s\n", INDENT_VALUE, name.c_str());
                    printAST({ entry });
                }
            } else if (auto bitfieldNode = dynamic_cast<ASTNodeBitfield*>(node); bitfieldNode != nullptr) {
                printf("%*c ASTNodeBitfield\n", INDENT_VALUE);

                for (const auto &[name, entry] : bitfieldNode->getEntries()) {
                    printf("%*c %s : \n", INDENT_VALUE, name.c_str());
                    printAST({ entry });
                }
            } else if (auto rvalueNode = dynamic_cast<ASTNodeRValue*>(node); rvalueNode != nullptr) {
                printf("%*c ASTNodeRValue\n", INDENT_VALUE);

                printf("%*c ", INDENT_VALUE);
                for (const auto &path : rvalueNode->getPath())
                    printf("%s.", path.c_str());
                printf("\n");
            } else {
                printf("%*c Invalid AST node!\n", INDENT_VALUE);
            }
        }
        indent -= 2;

        #undef INDENT_VALUE
    #endif
    }

}