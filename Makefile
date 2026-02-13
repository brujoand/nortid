CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I$(SRC_DIR)

UNITY_SRC = tests/unity/src/unity.c
SRC_DIR = src/c
TEST_DIR = tests

LANG_SRCS = $(SRC_DIR)/lang/lang.c $(SRC_DIR)/lang/lang_no.c $(SRC_DIR)/lang/lang_da.c $(SRC_DIR)/lang/lang_sv.c

.PHONY: test test-time test-date clean

test: test-time test-date

test-time: $(TEST_DIR)/test_time2words
	@./$(TEST_DIR)/test_time2words

test-date: $(TEST_DIR)/test_date2words
	@./$(TEST_DIR)/test_date2words

$(TEST_DIR)/test_time2words: $(TEST_DIR)/test_time2words.c $(SRC_DIR)/time2words.c $(LANG_SRCS) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_DIR)/test_date2words: $(TEST_DIR)/test_date2words.c $(SRC_DIR)/date2words.c $(LANG_SRCS) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TEST_DIR)/test_time2words $(TEST_DIR)/test_date2words
