package com.example.videoapp.data.remote

import com.google.gson.annotations.SerializedName
import retrofit2.Response

class BaseException(
        val errorType: ErrorType,
        val serverErrorResponse: ServerErrorResponse? = null,
        val response: Response<*>? = null,
        val httpCode: Int = 0,
        cause: Throwable? = null
) : RuntimeException(cause?.message, cause) {

    override val message: String?
        get() = when (errorType) {
            ErrorType.HTTP -> response?.message()

            ErrorType.NETWORK -> cause?.message

            ErrorType.SERVER -> serverErrorResponse?.message // TODO update real response

            ErrorType.UNEXPECTED -> cause?.message
        }

    companion object {
        fun toHttpError(response: Response<*>, httpCode: Int) =
                BaseException(
                        errorType = ErrorType.HTTP,
                        response = response,
                        httpCode = httpCode
                )

        fun toNetworkError(cause: Throwable) =
                BaseException(
                        errorType = ErrorType.NETWORK,
                        cause = cause
                )

        fun toServerError(serverErrorResponse: ServerErrorResponse, httpCode: Int) =
                BaseException(
                        errorType = ErrorType.SERVER,
                        serverErrorResponse = serverErrorResponse,
                        httpCode = httpCode
                )

        fun toUnexpectedError(cause: Throwable) =
                BaseException(
                        errorType = ErrorType.UNEXPECTED,
                        cause = cause
                )
    }
}

/**
 * Identifies the error type which triggered a [BaseException]
 */
enum class ErrorType {
    /**
     * An [IOException] occurred while communicating to the server.
     */
    NETWORK,

    /**
     * A non-2xx HTTP status code was received from the server.
     */
    HTTP,

    /**
     * A error server with code & message
     */
    SERVER,

    /**
     * An internal error occurred while attempting to execute a request. It is best practice to
     * re-throw this exception so your application crashes.
     */
    UNEXPECTED
}

// TODO update server error response
data class ServerErrorResponse(
        @SerializedName("message") val message: String? = null
)