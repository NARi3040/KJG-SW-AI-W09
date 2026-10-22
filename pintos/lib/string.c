#include <string.h>
#include <debug.h>

/* SRC에서 DST로 SIZE 바이트를 복사함.  두 영역은 겹치면 안 됨.
   DST를 반환함. */
void *
memcpy (void *dst_, const void *src_, size_t size) {
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT (dst != NULL || size == 0);
	ASSERT (src != NULL || size == 0);

	while (size-- > 0)
		*dst++ = *src++;

	return dst_;
}

/* SRC에서 DST로 SIZE 바이트를 복사함.  두 영역이 겹쳐도
   됨.  DST를 반환함. */
void *
memmove (void *dst_, const void *src_, size_t size) {
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT (dst != NULL || size == 0);
	ASSERT (src != NULL || size == 0);

	if (dst < src) {
		while (size-- > 0)
			*dst++ = *src++;
	} else {
		dst += size;
		src += size;
		while (size-- > 0)
			*--dst = *--src;
	}

	return dst;
}

/* A와 B에 있는 SIZE 바이트짜리 두 블록(block)에서 처음으로
   다른 바이트를 찾음.  A의 바이트가 더 크면 양수를, B의 바이트가
   더 크면 음수를, 두 블록이 같으면 0을 반환함. */
int
memcmp (const void *a_, const void *b_, size_t size) {
	const unsigned char *a = a_;
	const unsigned char *b = b_;

	ASSERT (a != NULL || size == 0);
	ASSERT (b != NULL || size == 0);

	for (; size-- > 0; a++, b++)
		if (*a != *b)
			return *a > *b ? +1 : -1;
	return 0;
}

/* 문자열 A와 B에서 처음으로 다른 문자를 찾음.
   A의 문자(unsigned char로 본 값)가 더 크면 양수를, B의 문자
   (unsigned char로 본 값)가 더 크면 음수를, 두 문자열이
   같으면 0을 반환함. */
int
strcmp (const char *a_, const char *b_) {
	const unsigned char *a = (const unsigned char *) a_;
	const unsigned char *b = (const unsigned char *) b_;

	ASSERT (a != NULL);
	ASSERT (b != NULL);

	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}

	return *a < *b ? -1 : *a > *b;
}

/* BLOCK에서 시작하는 처음 SIZE 바이트 안에서 CH가 처음 나타나는
   위치의 포인터를 반환함.  BLOCK 안에 CH가 없으면 널 포인터
   (null pointer)를 반환함. */
void *
memchr (const void *block_, int ch_, size_t size) {
	const unsigned char *block = block_;
	unsigned char ch = ch_;

	ASSERT (block != NULL || size == 0);

	for (; size-- > 0; block++)
		if (*block == ch)
			return (void *) block;

	return NULL;
}

/* STRING에서 C가 처음 나타나는 위치를 찾아 반환하며, STRING에
   C가 없으면 널 포인터(null pointer)를 반환함.  C == '\0'이면
   STRING 끝의 널 종결 문자(null terminator)를 가리키는 포인터를
   반환함. */
char *
strchr (const char *string, int c_) {
	char c = c_;

	ASSERT (string);

	for (;;)
		if (*string == c)
			return (char *) string;
		else if (*string == '\0')
			return NULL;
		else
			string++;
}

/* STRING의 앞부분 중 STOP에 없는 문자로만 이루어진
   부분 문자열(substring)의 길이를 반환함. */
size_t
strcspn (const char *string, const char *stop) {
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr (stop, string[length]) != NULL)
			break;
	return length;
}

/* STRING에서 STOP에도 들어 있는 첫 번째 문자의 포인터를 반환함.
   STRING의 어떤 문자도 STOP에 없으면 널 포인터(null pointer)를
   반환함. */
char *
strpbrk (const char *string, const char *stop) {
	for (; *string != '\0'; string++)
		if (strchr (stop, *string) != NULL)
			return (char *) string;
	return NULL;
}

/* STRING에서 C가 마지막으로 나타나는 위치의 포인터를 반환함.
   STRING에 C가 없으면 널 포인터(null pointer)를 반환함. */
char *
strrchr (const char *string, int c_) {
	char c = c_;
	const char *p = NULL;

	for (; *string != '\0'; string++)
		if (*string == c)
			p = string;
	return (char *) p;
}

/* STRING의 앞부분 중 SKIP에 있는 문자로만 이루어진
   부분 문자열(substring)의 길이를 반환함. */
size_t
strspn (const char *string, const char *skip) {
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr (skip, string[length]) == NULL)
			break;
	return length;
}

/* HAYSTACK 안에서 NEEDLE이 처음 나타나는 위치의 포인터를 반환함.
   HAYSTACK 안에 NEEDLE이 없으면 널 포인터(null pointer)를
   반환함. */
char *
strstr (const char *haystack, const char *needle) {
	size_t haystack_len = strlen (haystack);
	size_t needle_len = strlen (needle);

	if (haystack_len >= needle_len) {
		size_t i;

		for (i = 0; i <= haystack_len - needle_len; i++)
			if (!memcmp (haystack + i, needle, needle_len))
				return (char *) haystack + i;
	}

	return NULL;
}

/* 문자열을 DELIMITERS로 구분되는 토큰(token)으로 나눔.  이 함수를
   처음 호출할 때는 S가 토큰화할 문자열이어야 하고, 이후 호출에서는
   반드시 널 포인터(null pointer)여야 함.  SAVE_PTR은 토크나이저
   (tokenizer)의 위치를 기억하는 데 쓰는 `char *' 변수의 주소임.
   매 호출의 반환값은 문자열의 다음 토큰이며, 남은 토큰이 없으면
   널 포인터임.

   이 함수는 인접한 구분자(delimiter) 여러 개를 하나의 구분자로
   취급함.  반환되는 토큰의 길이는 절대 0이 아님.
   DELIMITERS는 하나의 문자열을 처리하는 도중에도 호출마다
   바뀔 수 있음.

   strtok_r()은 문자열 S를 수정하여 구분자를 널 바이트(null byte)로
   바꿈.  따라서 S는 수정 가능한 문자열이어야 함.  특히 문자열
   리터럴(string literal)은 하위 호환성을 위해 `const'가 아니긴 하지만
   C에서 수정할 수 *없음*.

   사용 예:

   char s[] = "  String to  tokenize. ";
   char *token, *save_ptr;

   for (token = strtok_r (s, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr))
	printf ("'%s'\n", token);

출력:

'String'
'to'
'tokenize.'
*/
char *
strtok_r (char *s, const char *delimiters, char **save_ptr) {
	char *token;

	ASSERT (delimiters != NULL);
	ASSERT (save_ptr != NULL);

	/* S가 널이 아니면 S부터 시작함.
	   S가 널이면 저장된 위치부터 시작함. */
	if (s == NULL)
		s = *save_ptr;
	ASSERT (s != NULL);

	/* 현재 위치에 있는 DELIMITERS를 모두 건너뜀. */
	while (strchr (delimiters, *s) != NULL) {
		/* 널 바이트(null byte)를 찾는 경우 strchr()는 항상 널이 아닌 값을
		   반환함.  모든 문자열은 (끝에) 널 바이트를 포함하기
		   때문임. */
		if (*s == '\0') {
			*save_ptr = s;
			return NULL;
		}

		s++;
	}

	/* 문자열 끝까지 DELIMITERS가 아닌 문자를 모두 건너뜀. */
	token = s;
	while (strchr (delimiters, *s) == NULL)
		s++;
	if (*s != '\0') {
		*s = '\0';
		*save_ptr = s + 1;
	} else
		*save_ptr = s;
	return token;
}

/* DST의 SIZE 바이트를 모두 VALUE로 설정함. */
void *
memset (void *dst_, int value, size_t size) {
	unsigned char *dst = dst_;

	ASSERT (dst != NULL || size == 0);

	while (size-- > 0)
		*dst++ = value;

	return dst_;
}

/* STRING의 길이를 반환함. */
size_t
strlen (const char *string) {
	const char *p;

	ASSERT (string);

	for (p = string; *p != '\0'; p++)
		continue;
	return p - string;
}

/* STRING의 길이가 MAXLEN 문자보다 짧으면 실제 길이를 반환함.
   그렇지 않으면 MAXLEN을 반환함. */
size_t
strnlen (const char *string, size_t maxlen) {
	size_t length;

	for (length = 0; string[length] != '\0' && length < maxlen; length++)
		continue;
	return length;
}

/* 문자열 SRC를 DST로 복사함.  SRC가 SIZE - 1 문자보다 길면
   SIZE - 1 문자만 복사됨.  SIZE가 0인 경우를 제외하면 DST에는
   항상 널 종결 문자(null terminator)가 기록됨.
   널 종결 문자를 제외한 SRC의 길이를 반환함.

   strlcpy()는 표준 C 라이브러리에 없지만 점점 널리 쓰이는
   확장 함수임.  strlcpy()에 대한 정보는
http://www.courtesan.com/todd/papers/strlcpy.html 참고. */
size_t
strlcpy (char *dst, const char *src, size_t size) {
	size_t src_len;

	ASSERT (dst != NULL);
	ASSERT (src != NULL);

	src_len = strlen (src);
	if (size > 0) {
		size_t dst_len = size - 1;
		if (src_len < dst_len)
			dst_len = src_len;
		memcpy (dst, src, dst_len);
		dst[dst_len] = '\0';
	}
	return src_len;
}

/* 문자열 SRC를 DST 뒤에 이어 붙임.  이어 붙인 문자열은
   SIZE - 1 문자로 제한됨.  SIZE가 0인 경우를 제외하면 DST에는
   항상 널 종결 문자(null terminator)가 기록됨.  공간이 충분하다고
   가정했을 때 이어 붙인 문자열이 가질 길이를 (널 종결 문자 제외)
   반환함.

   strlcat()은 표준 C 라이브러리에 없지만 점점 널리 쓰이는
   확장 함수임.  strlcpy()에 대한 정보는
http://www.courtesan.com/todd/papers/strlcpy.html 참고. */
size_t
strlcat (char *dst, const char *src, size_t size) {
	size_t src_len, dst_len;

	ASSERT (dst != NULL);
	ASSERT (src != NULL);

	src_len = strlen (src);
	dst_len = strlen (dst);
	if (size > 0 && dst_len < size) {
		size_t copy_cnt = size - dst_len - 1;
		if (src_len < copy_cnt)
			copy_cnt = src_len;
		memcpy (dst + dst_len, src, copy_cnt);
		dst[dst_len + copy_cnt] = '\0';
	}
	return src_len + dst_len;
}

