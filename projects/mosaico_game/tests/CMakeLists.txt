cmake_minimum_required(VERSION 3.20)
project(mosaico_chess_tests LANGUAGES C)

enable_testing()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

add_executable(chess_rules
    test_chess.c
    ../main/chess.c)
target_include_directories(chess_rules PRIVATE ../main)
target_compile_features(chess_rules PRIVATE c_std_11)
target_compile_options(chess_rules PRIVATE -Wall -Wextra -Werror)

add_test(NAME chess_rules COMMAND chess_rules)

add_executable(game_session
    test_game_session.c
    ../main/game_session.c
    ../main/chess.c)
target_include_directories(game_session PRIVATE ../main)
target_compile_features(game_session PRIVATE c_std_11)
target_compile_options(game_session PRIVATE -Wall -Wextra -Werror)

add_test(NAME game_session COMMAND game_session)
add_test(NAME gsp_contract
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_gsp_contract.py)

add_executable(game_flows
    test_game_flows.c
    ../main/game_session.c
    ../main/chess.c)
target_include_directories(game_flows PRIVATE ../main)
target_compile_features(game_flows PRIVATE c_std_11)
target_compile_options(game_flows PRIVATE -Wall -Wextra -Werror)

add_test(NAME game_flows COMMAND game_flows)

add_executable(chess_peer_protocol
    test_chess_peer_protocol.c
    ../main/chess_peer_protocol.c
    ../main/chess.c)
target_include_directories(chess_peer_protocol PRIVATE ../main)
target_compile_features(chess_peer_protocol PRIVATE c_std_11)
target_compile_options(chess_peer_protocol PRIVATE -Wall -Wextra -Werror)

add_test(NAME chess_peer_protocol COMMAND chess_peer_protocol)
