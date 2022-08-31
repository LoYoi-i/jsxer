#include "LogicalExpression.h"

string LogicalExpression::parenthesis(AstNode *node){
    switch (node->type()) {
        case NodeType::LocalAssignmentExpression:
        case NodeType::AssignmentExpression:
            return "(" + node->to_string() + ")";
        default:
            return node->to_string();
    }
}


void LogicalExpression::parse() {
    // opName is either "&&" or "||"
    opName = decoders::d_sid(reader);
    leftExpr = decoders::d_node(reader);
    rightExpr = decoders::d_node(reader);
    leftLiteral = decoders::d_variant(reader);
    rightLiteral = decoders::d_variant(reader);
}

string LogicalExpression::to_string() {

//    fprintf(stdout, "L: %s  R: %s \n", leftExpr->to_string().c_str(), rightExpr->to_string().c_str());

//    return get_expr(leftExpr, leftLiteral) + ' ' + opName + ' ' + get_expr(rightExpr, rightLiteral);

//    if (opName == "&&") {
//        if (rightExpr != nullptr && rightExpr->type() == NodeType::LogicalExpression){
//            LogicalExpression* right = (LogicalExpression *) rightExpr;
//            if (right->opName == "||")
//                return get_expr(leftExpr, leftLiteral) + " " + opName + " (" + right->to_string() + ")";
//        } else if (leftExpr != nullptr && leftExpr->type() == NodeType::LogicalExpression) {
//            LogicalExpression* left = (LogicalExpression *) leftExpr;
//            if (left->opName == "||")
//                return "(" + left->to_string() + ") " + opName + " " + get_expr(rightExpr, rightLiteral);
//        }
//    }
    string result = " " + opName + " ";

//    if (rightExpr != nullptr && rightExpr->type() == NodeType::LogicalExpression){
//        LogicalExpression* right = (LogicalExpression *) rightExpr;
//
//        if (right->opName == "||")
//            return get_expr(leftExpr, leftLiteral) + " " + opName + " (" + right->to_string() + ")";
//
//    } else if (leftExpr != nullptr && leftExpr->type() == NodeType::LogicalExpression && opName == "&&") {
//        LogicalExpression* left = (LogicalExpression *) leftExpr;
//
//        if (left->opName == "||")
//            return "(" + left->to_string() + ") " + opName + " " + get_expr(rightExpr, rightLiteral);
//    } else {
//        result = get_expr(leftExpr, leftLiteral) + result + get_expr(rightExpr, rightLiteral);
//    }

    if (leftExpr != nullptr) {
        if (leftExpr->type() == NodeType::LogicalExpression) {
            LogicalExpression* left = (LogicalExpression *) leftExpr;
            if (left->opName == "||" && opName == "&&") {
                result = "(" + left->to_string() + ")" + result;
                if (rightExpr != nullptr && rightExpr->type() == NodeType::LogicalExpression) {
                    LogicalExpression* right = (LogicalExpression *) rightExpr;
                    if (right->opName == "||"){
                        result += "(" + right->to_string() + ")";
                        goto done;
                    }
                }
            }
            else
                result = left->to_string() + result;
        } else {
            result = parenthesis(leftExpr) + result;
        }
    } else {
        result = leftLiteral + result;
    }

    if (rightExpr != nullptr) {
        if (rightExpr->type() == NodeType::LogicalExpression) {
            LogicalExpression* right = (LogicalExpression *) rightExpr;
            if (right->opName == "||")
                result += "(" + right->to_string() + ")";
            else
                result += right->to_string();
        } else {
            result += parenthesis(rightExpr);
        }
    } else {
        result += rightLiteral;
    }

    done:
    return result;
}
