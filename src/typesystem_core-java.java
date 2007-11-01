package generator;

import com.trolltech.qt.*;
import com.trolltech.qt.core.*;

class QObject___ extends QObject {
    @com.trolltech.qt.QtBlockedSlot
    public final java.util.List<QObject> findChildren() {
        return findChildren(null, (QRegExp) null);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final java.util.List<QObject> findChildren(Class<?> cl) {
        return findChildren(cl, (QRegExp) null);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final java.util.List<QObject> findChildren(Class<?> cl, String name) {
        return com.trolltech.qt.QtJambiInternal.findChildren(this, cl, name);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final java.util.List<QObject> findChildren(Class<?> cl, QRegExp name) {
        return com.trolltech.qt.QtJambiInternal.findChildren(this, cl, name);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final QObject findChild() {
        return findChild(null, null);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final QObject findChild(Class<?> cl) {
        return findChild(cl, null);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final QObject findChild(Class<?> cl, String name) {
        return com.trolltech.qt.QtJambiInternal.findChild(this, cl, name);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final void setProperty(String name, Object value) 
    {
        setProperty(QNativePointer.createCharPointer(name), value);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final Object property(String name) 
    {
        return property(QNativePointer.createCharPointer(name));
    }

    @com.trolltech.qt.QtBlockedSlot
    public final java.util.List<com.trolltech.qt.QtProperty> properties() 
    {
        return com.trolltech.qt.QtJambiInternal.properties(nativeId());
    }


    @com.trolltech.qt.QtBlockedSlot
    public final void connectSlotsByName() {
        com.trolltech.qt.QtJambiInternal.connectSlotsByName(this);
    }
}// class

abstract class QAbstractItemModel___ extends QAbstractItemModel {
    private native boolean setData_native(long id, int row, int col, Object value, int role);

    public final boolean setData(int row, int col, Object value) {
        return setData_native(nativeId(), row, col, value, com.trolltech.qt.core.Qt.ItemDataRole.DisplayRole);
    }

    public final boolean setData(int row, int col, Object value, int role) {
        return setData_native(nativeId(), row, col, value, role);
    }

    private native Object data_native(long id, int row, int col, int role);

    public final Object data(int row, int col, int role) {
        return data_native(nativeId(), row, col, role);
    }

    public final Object data(int row, int col) {
        return data_native(nativeId(), row, col, Qt.ItemDataRole.DisplayRole);
    }
}// class

class QTimer___ extends QTimer {
    static private class QSingleShotTimer extends QObject {
        private int timerId = -1;
        public Signal0 timeout = new Signal0();

        public QSingleShotTimer(int msec, QObject obj, String method) {
            super(obj);
            timeout.connect(obj, method);
            timerId = startTimer(msec);
        }

        protected void disposed() {
            if (timerId > 0)
                killTimer(timerId);
            super.disposed();
        }

        protected void timerEvent(QTimerEvent e) {
            if (timerId > 0)
                killTimer(timerId);
            timerId = -1;
            timeout.emit();
            disposeLater();
        }
    }

    public static void singleShot(int msec, QObject obj, String method) {
        new QSingleShotTimer(msec, obj, method);
    }
}// class

class QCoreApplication___ extends QCoreApplication {

    protected static QCoreApplication m_instance = null;

    public QCoreApplication(String args[]) {
        this(argc(args), argv(args));
    }

    public static String translate(String context, String sourceText, String comment) {
        QTextCodec codec = QTextCodec.codecForName("UTF-8");
        return translate(context != null ? codec.fromUnicode(context).data() : null, sourceText != null ? codec.fromUnicode(sourceText).data() : null,
                comment != null ? codec.fromUnicode(comment).data() : null, Encoding.CodecForTr);
    }

    public static String translate(String context, String sourceText) {
        return translate(context, sourceText, null);
    }

    public static String translate(String context, String sourceText, String comment, int n) {
        QTextCodec codec = QTextCodec.codecForName("UTF-8");
        return translate(context != null ? codec.fromUnicode(context).data() : null, sourceText != null ? codec.fromUnicode(sourceText).data() : null,
                comment != null ? codec.fromUnicode(comment).data() : null, Encoding.CodecForTr, n);
    }

    public static void initialize(String args[]) {
        if (m_instance != null)
            throw new RuntimeException("QApplication can only be initialized once");

        m_instance = new QCoreApplication(args);
        m_instance.aboutToQuit.connect(m_instance, "disposeOfMyself()");
        String path = Utilities.unpackPlugins();
        if (path != null)
            addLibraryPath(path);
        else
            QtJambiInternal.setupDefaultPluginPath();
    }

    @SuppressWarnings("unused")
    private void disposeOfMyself() {
        m_instance = null;
        System.gc();
        this.dispose();
    }

    protected final static com.trolltech.qt.QNativePointer argv(String args[]) {
        String newArgs[] = new String[args.length + 1];
        System.arraycopy(args, 0, newArgs, 1, args.length);
        newArgs[0] = "Qt Jambi application";
        argv = com.trolltech.qt.QNativePointer.createCharPointerPointer(newArgs);
        return argv;
    }

    protected final static com.trolltech.qt.QNativePointer argc(String args[]) {
        if (argc != null) {
            throw new RuntimeException("There can only exist one QCoreApplication instance");
        }
        argc = new com.trolltech.qt.QNativePointer(com.trolltech.qt.QNativePointer.Type.Int);
        argc.setIntValue(args.length + 1);
        return argc;
    }

    @Override
    protected void disposed() {
        argc = null;
        argv = null;
        m_instance = null;
        super.disposed();
    }

    public static void invokeLater(java.lang.Runnable runnable) {
        postEvent(new QInvokable(runnable), new QEvent(QInvokable.INVOKABLE_EVENT));
    }

    /**
     * Executes the runnable's run() method in the main thread and waits for it
     * to return. If the current thread is not the main thread, an event loop
     * must be running in the main thread, or this method will wait
     * indefinitely.
     */
    public static void invokeAndWait(Runnable runnable) {
        QSynchronousInvokable invokable = new QSynchronousInvokable(runnable);
        QCoreApplication.postEvent(invokable, new QEvent(QSynchronousInvokable.SYNCHRONOUS_INVOKABLE_EVENT));
        invokable.waitForInvoked();
        invokable.disposeLater();
    }

    private static com.trolltech.qt.QNativePointer argc, argv;

}// class

class QTranslator___ extends QTranslator {
    public final boolean load(byte data[]) {
        return load(com.trolltech.qt.QtJambiInternal.byteArrayToNativePointer(data), data.length);
    }
}// class

class QProcess___ extends QProcess {

    public static class DetachedProcessInfo {
        public DetachedProcessInfo(boolean success, long pid) {
            this.success = success;
            this.pid = pid;
        }

        public boolean success;
        public long pid;
    }

    public static DetachedProcessInfo startDetached(String program, java.util.List<String> arguments, String workingDirectory) {
        QNativePointer pid = new QNativePointer(QNativePointer.Type.Long);
        boolean success = startDetached(program, arguments, workingDirectory, pid);
        return new DetachedProcessInfo(success, pid.longValue());
    }
}// class

class QDataStream___ extends QDataStream {

    private QNativePointer srb = new QNativePointer(QNativePointer.Type.Byte, 32) {
        {
            setVerificationEnabled(false);
        }
    };

    public final boolean readBoolean() {
        operator_shift_right_boolean(srb);
        return srb.booleanValue();
    }

    public final byte readByte() {
        operator_shift_right_byte(srb);
        return srb.byteValue();
    }

    public final short readShort() {
        operator_shift_right_short(srb);
        return srb.shortValue();
    }

    public final int readInt() {
        operator_shift_right_int(srb);
        return srb.intValue();
    }

    public final long readLong() {
        operator_shift_right_long(srb);
        return srb.longValue();
    }

    public final float readFloat() {
        operator_shift_right_float(srb);
        return srb.floatValue();
    }

    public final double readDouble() {
        operator_shift_right_double(srb);
        return srb.doubleValue();
    }

    public final QDataStream writeShort(short s) {
        writeShort_char((char) s);
        return this;
    }

    private native String readString_private(long nativeId);

    private native void writeString_private(long nativeId, String string);

    public final String readString() {
        if (nativeId() == 0)
            throw new QNoNativeResourcesException("Function call on incomplete object of type: " + getClass().getName());
        return readString_private(nativeId());
    }

    public final void writeString(String string) {
        if (nativeId() == 0)
            throw new QNoNativeResourcesException("Function call on incomplete object of type: " + getClass().getName());
        writeString_private(nativeId(), string);
    }

    private native int writeBytes(long id, byte buffer[], int length);

    private native int readBytes(long id, byte buffer[], int length);

    public final int writeBytes(byte buffer[]) {
        return writeBytes(buffer, buffer.length);
    }

    public final int writeBytes(byte buffer[], int length) {
        return writeBytes(nativeId(), buffer, length);
    }

    public final int readBytes(byte buffer[]) {
        return readBytes(buffer, buffer.length);
    }

    public final int readBytes(byte buffer[], int length) {
        return readBytes(nativeId(), buffer, length);
    }
}// class

class QTextStream___ extends QTextStream {
    public final void setCodec(String codecName) {
        setCodec(QNativePointer.createCharPointer(codecName));
        if (codec() != __rcCodec)
            __rcCodec = null;
    }

    private QNativePointer srb = new QNativePointer(QNativePointer.Type.Byte, 32) {
        {
            setVerificationEnabled(false);
        }
    };

    public final byte readByte() {
        operator_shift_right_byte(srb);
        return srb.byteValue();
    }

    public final short readShort() {
        operator_shift_right_short(srb);
        return srb.shortValue();
    }

    public final int readInt() {
        operator_shift_right_int(srb);
        return srb.intValue();
    }

    public final long readLong() {
        operator_shift_right_long(srb);
        return srb.longValue();
    }

    public final float readFloat() {
        operator_shift_right_float(srb);
        return srb.floatValue();
    }

    public final double readDouble() {
        operator_shift_right_double(srb);
        return srb.doubleValue();
    }

    public final QTextStream writeShort(short s) {
        writeShort_char((char) s);
        return this;
    }

    public final String readString() {
        return readString_native(nativeId());
    }

    public final void writeString(String string) {
        writeString_native(nativeId(), string);
    }

    private final native String readString_native(long id);

    private final native void writeString_native(long id, String string);

}// class

class QBitArray___ extends QBitArray {

    @com.trolltech.qt.QtBlockedSlot
    public final void xor(QBitArray other) {
        operator_xor_assign(other);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final void and(QBitArray other) {
        operator_and_assign(other);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final void or(QBitArray other) {
        operator_or_assign(other);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final void set(QBitArray other) {
        operator_assign(other);
    }

    @com.trolltech.qt.QtBlockedSlot
    public final QBitArray inverted() {
        return operator_negate();
    }

}// class

// hfr

class QDate___ extends QDate {

    public final int weekNumber() {
        return weekNumber(null);
    }

    public final int yearOfWeekNumber() {
        QNativePointer np = new QNativePointer(QNativePointer.Type.Int);
        weekNumber(np);
        return np.intValue();
    }

}// class

class QDir___ extends QDir {

    @com.trolltech.qt.QtBlockedSlot
    public String at(int i) {
        return operator_subscript(i);
    }

}// class

class QByteArray___ extends QByteArray {
    public QByteArray(String s) {
        this();
        append(s);
    }

    public QByteArray(byte data[]) {
        this(com.trolltech.qt.QtJambiInternal.byteArrayToNativePointer(data), data.length);
    }

    public final boolean contains(String str) {
        return contains(new QByteArray(str));
    }

    public final int count(String str) {
        return count(new QByteArray(str));
    }

    public final boolean endsWith(String str) {
        return endsWith(new QByteArray(str));
    }

    public final QByteArray prepend(String str) {
        return prepend(new QByteArray(str));
    }

    public final QByteArray replace(QByteArray before, String after) {
        return replace(before, new QByteArray(after));
    }

    public final QByteArray replace(String before, String after) {
        return replace(new QByteArray(before), new QByteArray(after));
    }

    public final boolean startsWith(String str) {
        return startsWith(new QByteArray(str));
    }

    public final byte[] toByteArray() {
        byte[] res = new byte[size()];

        for (int i = 0; i < size(); i++) {
            res[i] = at(i);
        }
        return res;
    }

    @com.trolltech.qt.QtBlockedSlot
    public final QByteArray set(QByteArray other) {
        operator_assign(other);
        return this;
    }

}// class

class QFile___ extends QFile {

    public static String decodeName(String localFileName) {
        return decodeName(com.trolltech.qt.QNativePointer.createCharPointer(localFileName));
    }

}// class

class QIODevice___ extends QIODevice {

    /**
     * Gets a byte from the device.
     * 
     * @return -1 on failure, or the value of the byte on success
     */
    public final int getByte() {
        QNativePointer np = new QNativePointer(QNativePointer.Type.Byte);
        boolean success = getByte(np);
        return success ? np.byteValue() : -1;
    }

}// class

class QCryptographicHash___ extends QCryptographicHash {

    public final void addData(byte data[]) {
        QNativePointer np = com.trolltech.qt.QtJambiInternal.byteArrayToNativePointer(data);
        addData(np, data.length);
    }

}// class

class QTextCodec___ extends QTextCodec {

    static {
        setCodecForTr(QTextCodec.codecForName("UTF-8"));
    }

    public static QTextCodec codecForName(String name) {
        return codecForName(com.trolltech.qt.QNativePointer.createCharPointer(name));
    }

}// class

class QBuffer___ extends QBuffer {

    // retain a reference to avoid gc
    @SuppressWarnings("unused")
    private Object strongDataReference = null;

    public QBuffer(QByteArray byteArray, QObject parent) {
        this(byteArray.nativePointer(), parent);
        strongDataReference = byteArray;
    }

    public QBuffer(QByteArray byteArray) {
        this(byteArray, null);
    }

    public final void setBuffer(QByteArray byteArray) {
        setBuffer(byteArray.nativePointer());
        strongDataReference = byteArray;
    }

    public final void setData(byte data[]) {
        QNativePointer np = com.trolltech.qt.QtJambiInternal.byteArrayToNativePointer(data);
        setData(np, data.length);
    }

}// class

class QSignalMapper___ extends QSignalMapper {
    
    private java.util.Hashtable<QObject, QObject> __rcObjectForObject = new java.util.Hashtable<QObject, QObject>();
    
}// class
